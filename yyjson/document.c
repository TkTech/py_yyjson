#include "document.h"

#include "memory.h"
#include "decimal.h"
#include "pathlib.h"

static PyObject *mut_element_to_primitive(yyjson_mut_val *val, int depth);
static PyObject *element_to_primitive(yyjson_val *val, int depth);

/**
 * Return non-zero if the buffer is pure ASCII. Only the yes/no answer is needed
 * (not a character count), so this checks a word at a time.
 */
static inline int is_ascii(const char *src, size_t len) {
  size_t i = 0;
  for (; i + 8 <= len; i += 8) {
    uint64_t word;
    memcpy(&word, src + i, sizeof(word));
    if (word & 0x8080808080808080ULL) return 0;
  }
  for (; i < len; i++) {
    if ((unsigned char)src[i] & 0x80) return 0;
  }
  return 1;
}

/**
 * Convert the given UTF-8 string into a Python unicode object.
 */
PyObject *unicode_from_str(const char *src, size_t len) {
#ifndef PYPY_VERSION
  // Exploit the internals of CPython's unicode implementation to
  // implement a fast-path for ASCII data, which is by far the
  // most common case. This is the single greatest performance gain
  // of any optimization in this library.
  //
  // The details of these structures are here:
  //    https://github.com/python/cpython/blob/main/Include/cpython/unicodeobject.h#L53
  if (yyjson_likely(is_ascii(src, len))) {
    PyObject *uni = PyUnicode_New(len, 127);
    if (!uni) return NULL;
    PyASCIIObject *uni_ascii = (PyASCIIObject *)uni;
    memcpy(uni_ascii + 1, src, len);
    return uni;
  }
#endif

  return PyUnicode_DecodeUTF8(src, len, NULL);
}

/**
 * Open a path-like object for binary reading. Cross-platform: on Windows the
 * path must go through the wide-char API (fopen() interprets narrow paths in
 * the ANSI codepage, breaking non-ASCII names); elsewhere the path is encoded
 * with the filesystem encoding (not UTF-8 + str(), which breaks surrogate
 * names). Shared with the streaming (SAX) reader.
 *
 * Returns NULL with an OSError set on failure.
 *
 * TODO: replace with the public Py_fopen() once Python 3.14 is our floor.
 */
FILE *fopen_path(PyObject *path) {
  FILE *fp;
#ifdef MS_WINDOWS
  PyObject *str = NULL;
  wchar_t *wpath;
  if (!PyUnicode_FSDecoder(path, &str)) return NULL;
  wpath = PyUnicode_AsWideCharString(str, NULL);
  Py_DECREF(str);
  if (wpath == NULL) return NULL;
  fp = _wfopen(wpath, L"rb");
  if (fp == NULL) PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError, path);
  PyMem_Free(wpath);
#else
  PyObject *bytes = NULL;
  if (!PyUnicode_FSConverter(path, &bytes)) return NULL;
  fp = fopen(PyBytes_AS_STRING(bytes), "rb");
  if (fp == NULL) PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError, path);
  Py_DECREF(bytes);
#endif
  return fp;
}

/*
 * Object keys repeat heavily in real JSON (every record in an array of objects
 * shares the same key set), and re-decoding and re-hashing an identical
 * PyUnicode for each occurrence dominates conversion time. This direct-mapped
 * cache returns an existing, hash-cached key on a hit, skipping the decode, the
 * allocation and the hash.
 *
 * CPython only; PyPy falls back to creating a fresh key each time.
 */
#ifndef PYPY_VERSION

#define KEY_CACHE_SIZE 4096u  /* power of two */
#define KEY_CACHE_MAX_LEN 64  /* only short keys (identifiers) are cached */

/* A cache slot stores the interned key plus its UTF-8 bytes/length, so a hit is
   a length check + memcmp with no Python API call. `utf8` points into `obj`'s
   own cached UTF-8 buffer and stays valid until the slot is evicted. */
typedef struct {
  PyObject *obj;
  const char *utf8;
  uint32_t len;
} key_slot;

static key_slot key_cache[KEY_CACHE_SIZE];

/* Return a hash-cached PyUnicode for the given key bytes. New reference. */
static inline PyObject *cached_key(const char *str, size_t len) {
  PyObject *key;

  /* Empty or unusually long keys don't earn a cache slot, but still get their
     hash computed so the dict insert can reuse it. */
  if (len == 0 || len > KEY_CACHE_MAX_LEN) {
    key = unicode_from_str(str, len);
    if (key != NULL && ((PyASCIIObject *)key)->hash == -1) {
      if (PyObject_Hash(key) == -1) {
        Py_DECREF(key);
        return NULL;
      }
    }
    return key;
  }

  /* FNV-1a over the key bytes selects the slot. */
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < len; i++) {
    h ^= (unsigned char)str[i];
    h *= 16777619u;
  }
  uint32_t idx = h & (KEY_CACHE_SIZE - 1);

  key_slot *slot = &key_cache[idx];
  if (slot->obj != NULL && slot->len == (uint32_t)len &&
      memcmp(slot->utf8, str, len) == 0) {
    Py_INCREF(slot->obj);
    return slot->obj;
  }

  key = unicode_from_str(str, len);
  if (key == NULL) return NULL;
  /* Compute and store the hash now, so the dict insert (and every future hit)
     skips siphash. */
  if (PyObject_Hash(key) == -1) {
    Py_DECREF(key);
    return NULL;
  }

  /* Cache the object's UTF-8 view for future comparisons (free for ASCII). */
  {
    Py_ssize_t clen;
    const char *cbytes = PyUnicode_AsUTF8AndSize(key, &clen);
    if (cbytes == NULL) {
      Py_DECREF(key);
      return NULL;
    }
    Py_XSETREF(slot->obj, key);  /* evict previous occupant */
    slot->utf8 = cbytes;
    slot->len = (uint32_t)clen;
  }

  Py_INCREF(key);  /* one ref for the cache slot (above), one for the caller */
  return key;
}

/* Presize the dict to avoid resizes while filling. `cached_key` has already
   computed and stored each key's hash in the object, so a plain PyDict_SetItem
   reuses it (no siphash) -- we don't need the (3.13-removed) known-hash API. */
#define NEW_DICT(n) _PyDict_NewPresized((Py_ssize_t)(n))
#define DICT_SET_KEYVAL(d, k, v) PyDict_SetItem((d), (k), (v))

#else /* PYPY_VERSION */

#define cached_key(str, len) unicode_from_str((str), (len))
#define NEW_DICT(n) PyDict_New()
#define DICT_SET_KEYVAL(d, k, v) PyDict_SetItem((d), (k), (v))

#endif /* PYPY_VERSION */

/* Bound on conversion nesting depth. The recursion is a leaf C function with a
   small frame, so this is safe on any reasonable stack while still covering any
   realistic document. It also has to cover the freeze() path, where the value
   graph comes from an in-memory build rather than the parser, so a limit on the
   parser alone would not suffice. */
#define PY_YYJSON_MAX_DEPTH 1024

/* The converter body lives in element_to_primitive.h and is instantiated for
   both of yyjson's mirrored value APIs; see that file for the pattern. */

#define CONVERT_FN element_to_primitive
#define CONVERT_VAL yyjson_val
#define CONVERT_API(n) yyjson_##n
#include "element_to_primitive.h"
#undef CONVERT_FN
#undef CONVERT_VAL
#undef CONVERT_API

#define CONVERT_FN mut_element_to_primitive
#define CONVERT_VAL yyjson_mut_val
#define CONVERT_API(n) yyjson_mut_##n
#include "element_to_primitive.h"
#undef CONVERT_FN
#undef CONVERT_VAL
#undef CONVERT_API


PyTypeObject *type_for_conversion(PyObject *obj) {
  if (obj->ob_type == &PyUnicode_Type) {
    return &PyUnicode_Type;
  } else if (obj->ob_type == &PyLong_Type) {
    return &PyLong_Type;
  } else if (obj->ob_type == &PyFloat_Type) {
    return &PyFloat_Type;
  } else if (obj->ob_type == &PyDict_Type) {
    return &PyDict_Type;
  } else if (obj->ob_type == &PyList_Type) {
    return &PyList_Type;
  } else if (obj->ob_type == &PyTuple_Type) {
    return &PyTuple_Type;
  } else if (obj->ob_type == &PyBool_Type) {
    return &PyBool_Type;
  } else if (obj->ob_type == Py_None->ob_type) {
    return Py_None->ob_type;
  }
  return NULL;
}

/**
 * Recursively convert a Python object into yyjson elements.
 */
static inline yyjson_mut_val *mut_primitive_to_element(
    DocumentObject *self,
    yyjson_mut_doc *doc,
    PyObject *obj
) {
  const PyTypeObject *ob_type = type_for_conversion(obj);

  if (yyjson_unlikely(ob_type == NULL) && self->default_func != NULL) {
    PyObject *result = PyObject_CallFunctionObjArgs(self->default_func, obj, NULL);
    if (result == NULL) {
      return NULL;
    }
    if (Py_EnterRecursiveCall(" while converting a Python object to JSON")) {
      Py_DECREF(result);
      return NULL;
    }
    yyjson_mut_val *val = mut_primitive_to_element(self, doc, result);
    Py_LeaveRecursiveCall();
    Py_DECREF(result);
    return val;
  }

  if (ob_type == &PyUnicode_Type) {
    Py_ssize_t str_len;
    const char *str = PyUnicode_AsUTF8AndSize(obj, &str_len);
    return yyjson_mut_strncpy(doc, str, str_len);
  } else if (ob_type == &PyLong_Type) {
    // Serialization of integers is a little special, since Python allows
    // integers of (effectively) any size. While > 53bit is technically
    // against the spec, at least 64bit is widely supported and the builtin
    // Python JSON module supports integers of any size.
    int overflow = 0;
    const int64_t num = PyLong_AsLongLongAndOverflow(obj, &overflow);
    if (!overflow) {
      if (num == -1 && PyErr_Occurred()) return NULL;
      return yyjson_mut_sint(doc, num);
    } else {
      // Number overflowed, try an unsigned long long.
      const uint64_t unum = PyLong_AsUnsignedLongLong(obj);
      if (unum == (uint64_t)-1 && PyErr_Occurred()) {
        // Number might have been too large even for a unit64_t, resort
        // to a raw type by converting the number to its string
        // representation.
        PyErr_Clear();  // Erase the OverflowError
        PyObject *str_repr = PyObject_Str(obj);
        if (str_repr == NULL) return NULL;
        Py_ssize_t str_len;
        const char *str = PyUnicode_AsUTF8AndSize(str_repr, &str_len);
        if (str == NULL) {
          Py_DECREF(str_repr);
          return NULL;
        }
        yyjson_mut_val *val = yyjson_mut_rawncpy(doc, str, str_len);
        Py_DECREF(str_repr);
        return val;
      } else {
        return yyjson_mut_uint(doc, unum);
      }
    }
  } else if (ob_type == &PyList_Type) {
    if (Py_EnterRecursiveCall(" while converting a Python object to JSON")) {
      return NULL;
    }
    yyjson_mut_val *val = yyjson_mut_arr(doc);
    for (Py_ssize_t i = 0; i < PyList_GET_SIZE(obj); i++) {
      yyjson_mut_val *object_value =
          mut_primitive_to_element(self, doc, PyList_GET_ITEM(obj, i));
      if (yyjson_unlikely(object_value == NULL)) goto error;

      yyjson_mut_arr_append(val, object_value);
    }
    Py_LeaveRecursiveCall();
    return val;
  } else if (ob_type == &PyTuple_Type) {
    if (Py_EnterRecursiveCall(" while converting a Python object to JSON")) {
      return NULL;
    }
    yyjson_mut_val *val = yyjson_mut_arr(doc);
    for (Py_ssize_t i = 0; i < PyTuple_GET_SIZE(obj); i++) {
      yyjson_mut_val *object_value =
          mut_primitive_to_element(self, doc, PyTuple_GET_ITEM(obj, i));
      if (yyjson_unlikely(object_value == NULL)) goto error;

      yyjson_mut_arr_append(val, object_value);
    }
    Py_LeaveRecursiveCall();
    return val;
  } else if (ob_type == &PyDict_Type) {
    if (Py_EnterRecursiveCall(" while converting a Python object to JSON")) {
      return NULL;
    }
    yyjson_mut_val *val = yyjson_mut_obj(doc);
    Py_ssize_t i = 0;
    PyObject *key, *value;

    while (PyDict_Next(obj, &i, &key, &value)) {
      Py_ssize_t str_len;
      const char *str = PyUnicode_AsUTF8AndSize(key, &str_len);
      if (yyjson_unlikely(str == NULL)) {
        PyErr_SetString(PyExc_TypeError, "Dictionary keys must be strings");
        goto error;
      }
      yyjson_mut_val *object_value = mut_primitive_to_element(self, doc, value);
      if (yyjson_unlikely(object_value == NULL)) goto error;

      yyjson_mut_obj_add(
          val, yyjson_mut_strncpy(doc, str, str_len), object_value
      );
    }
    Py_LeaveRecursiveCall();
    return val;
  } else if (ob_type == &PyFloat_Type) {
    double dnum = PyFloat_AsDouble(obj);
    if (dnum == -1 && PyErr_Occurred()) return NULL;
    return yyjson_mut_real(doc, dnum);
  } else if (obj == Py_True) {
    return yyjson_mut_true(doc);
  } else if (obj == Py_False) {
    return yyjson_mut_false(doc);
  } else if (obj == Py_None) {
    return yyjson_mut_null(doc);
  } else if (yyjson_unlikely(PyObject_IsInstance(obj, YY_DecimalClass))) {
    PyObject *str_repr = PyObject_Str(obj);
    Py_ssize_t str_len;
    const char *str = PyUnicode_AsUTF8AndSize(str_repr, &str_len);
    yyjson_mut_val *val = yyjson_mut_rawncpy(doc, str, str_len);
    Py_DECREF(str_repr);
    return val;
  } else {
    PyErr_Format(PyExc_TypeError,
      "Object of type '%s' is not JSON serializable",
      Py_TYPE(obj)->tp_name
    );
    return NULL;
  }

error:
  Py_LeaveRecursiveCall();
  return NULL;
}

static void Document_dealloc(DocumentObject *self) {
  if (self->i_doc != NULL) yyjson_doc_free(self->i_doc);
  if (self->m_doc != NULL) yyjson_mut_doc_free(self->m_doc);
  Py_XDECREF(self->default_func);
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Document_new(
    PyTypeObject *type, PyObject *args, PyObject *kwds
) {
  DocumentObject *self;
  self = (DocumentObject *)type->tp_alloc(type, 0);

  if (self != NULL) {
    self->m_doc = NULL;
    self->i_doc = NULL;
    self->alc = &PyMem_Allocator;
  }

  return (PyObject *)self;
}

/**
 * Build the given (freshly-allocated) Document from a JSON-serializable Python
 * object. Unlike parsing, a ``str``/``bytes`` argument is serialized as a JSON
 * value rather than interpreted as JSON text. Returns 0 on success, -1 (with an
 * exception set) on failure.
 */
static int document_build_from_object(DocumentObject *self, PyObject *content) {
  self->m_doc = yyjson_mut_doc_new(self->alc);
  if (!self->m_doc) {
    PyErr_SetString(
        PyExc_ValueError, "Unable to create empty mutable document."
    );
    return -1;
  }

  yyjson_mut_val *val = mut_primitive_to_element(self, self->m_doc, content);
  if (val == NULL) {
    return -1;
  }

  yyjson_mut_doc_set_root(self->m_doc, val);
  return 0;
}

/**
 * Read a binary file-like object (``read``/``readinto``) fully into a buffer
 * and parse it into ``self->i_doc``. The whole stream is drained in chunks, so
 * any read()-able object works; note that a DOM inherently holds the entire
 * document, so peak memory is O(document) regardless. For bounded memory over a
 * huge stream, use the module-level ``sax()`` instead.
 *
 * Returns 0 on success, -1 (with an exception set) on failure.
 */
static int document_read_stream(
    DocumentObject *self, PyObject *fileobj, yyjson_read_flag flag
) {
  PyObject *readinto = PyObject_GetAttrString(fileobj, "readinto");
  PyObject *readm = NULL;
  char *buf = NULL;
  size_t cap = 0, len = 0;
  const size_t CHUNK = (size_t)1 << 16;
  yyjson_read_err err;

  if (!readinto) {
    PyErr_Clear();
    readm = PyObject_GetAttrString(fileobj, "read");
    if (!readm) {
      PyErr_Clear();
      PyErr_SetString(PyExc_TypeError,
                      "expected a binary file-like object with read()");
      return -1;
    }
  }

  /* If the stream is seekable, learn the remaining size and presize the
     buffer, so the whole stream is drained in one readinto() with no growth
     reallocs (the seek/tell protocol respects the object's own buffering,
     unlike fd-level sizing). A short or stale answer is harmless: the drain
     loop below still reads to EOF and grows if needed. Only for readinto
     streams: text-mode tell() returns opaque cookies, not byte offsets. */
  if (readinto) {
    PyObject *r = PyObject_CallMethod(fileobj, "seekable", NULL);
    int seekable = r ? PyObject_IsTrue(r) : 0;
    Py_XDECREF(r);
    if (seekable > 0) {
      PyObject *pos_o = PyObject_CallMethod(fileobj, "tell", NULL);
      PyObject *end_o =
          pos_o ? PyObject_CallMethod(fileobj, "seek", "ii", 0, 2) : NULL;
      if (end_o) {
        /* we moved the position; failing to restore it must propagate, or
           the drain below would silently read nothing from the tail */
        PyObject *back_o = PyObject_CallMethod(fileobj, "seek", "Oi", pos_o, 0);
        if (!back_o) {
          Py_DECREF(end_o);
          Py_DECREF(pos_o);
          Py_XDECREF(readinto);
          return -1;
        }
        Py_DECREF(back_o);
        {
          Py_ssize_t pos = PyNumber_AsSsize_t(pos_o, NULL);
          Py_ssize_t end = PyNumber_AsSsize_t(end_o, NULL);
          if (!PyErr_Occurred() && end > pos) {
            size_t hint = (size_t)(end - pos) + CHUNK + YYJSON_PADDING_SIZE;
            buf = (char *)self->alc->malloc(self->alc->ctx, hint);
            if (buf) cap = hint;
          }
        }
      }
      Py_XDECREF(end_o);
      Py_XDECREF(pos_o);
    }
    if (PyErr_Occurred()) PyErr_Clear();
  }

  for (;;) {
    size_t space;
    if (len + CHUNK + YYJSON_PADDING_SIZE > cap) {
      size_t ncap = cap ? cap * 2 : CHUNK * 2;
      char *nb;
      while (len + CHUNK + YYJSON_PADDING_SIZE > ncap) ncap *= 2;
      nb = (char *)self->alc->realloc(self->alc->ctx, buf, cap, ncap);
      if (!nb) { PyErr_NoMemory(); goto error; }
      buf = nb;
      cap = ncap;
    }
    space = cap - len - YYJSON_PADDING_SIZE;

    if (readinto) {
      PyObject *mv = PyMemoryView_FromMemory(buf + len, (Py_ssize_t)space,
                                             PyBUF_WRITE);
      PyObject *r;
      Py_ssize_t got;
      if (!mv) goto error;
      r = PyObject_CallOneArg(readinto, mv);
      Py_DECREF(mv);
      if (!r) goto error;
      if (r == Py_None) {
        /* None means "no data available right now" on a non-blocking
           stream, not EOF; treating it as EOF would silently truncate. */
        Py_DECREF(r);
        PyErr_SetString(PyExc_BlockingIOError,
                        "readinto() returned None (no data available on a "
                        "non-blocking stream); a blocking stream is required");
        goto error;
      }
      got = PyNumber_AsSsize_t(r, NULL);
      Py_DECREF(r);
      if (got < 0) {
        if (!PyErr_Occurred())
          PyErr_SetString(PyExc_ValueError, "readinto() returned < 0");
        goto error;
      }
      if (got == 0) break;
      len += (size_t)got;
    } else {
      PyObject *r = PyObject_CallFunction(readm, "n", (Py_ssize_t)space);
      char *data;
      Py_ssize_t got;
      if (!r) goto error;
      if (r == Py_None) {
        Py_DECREF(r);
        PyErr_SetString(PyExc_BlockingIOError,
                        "read() returned None (no data available on a "
                        "non-blocking stream); a blocking stream is required");
        goto error;
      }
      if (!PyBytes_Check(r)) {
        Py_DECREF(r);
        PyErr_SetString(PyExc_TypeError,
                        "read() must return bytes; open in binary mode");
        goto error;
      }
      if (PyBytes_AsStringAndSize(r, &data, &got) < 0) { Py_DECREF(r); goto error; }
      if (got == 0) { Py_DECREF(r); break; }
      if ((size_t)got > space) got = (Py_ssize_t)space;
      memcpy(buf + len, data, (size_t)got);
      Py_DECREF(r);
      len += (size_t)got;
    }
  }

  Py_XDECREF(readinto);
  Py_XDECREF(readm);

  if (len == 0) {
    if (buf) self->alc->free(self->alc->ctx, buf);
    PyErr_SetString(PyExc_ValueError, "no data read from stream");
    return -1;
  }

  /* Parse in place and hand `buf` to the document via `str_pool`, so it is
     freed by yyjson_doc_free. This skips the full copy a non-insitu read
     would make (the same pattern yyjson_read_fp uses). Shrink first: the
     buffer lives as long as the document, and growth doubling can leave up
     to 2x the input in unused capacity. */
  if (cap > len + YYJSON_PADDING_SIZE) {
    char *nb = (char *)self->alc->realloc(self->alc->ctx, buf, cap,
                                          len + YYJSON_PADDING_SIZE);
    if (nb) buf = nb; /* shrink failure is harmless; keep the larger buffer */
  }
  memset(buf + len, 0, YYJSON_PADDING_SIZE);
  self->i_doc =
      yyjson_read_opts(buf, len, flag | YYJSON_READ_INSITU, self->alc, &err);
  if (!self->i_doc) {
    self->alc->free(self->alc->ctx, buf);
    PyErr_SetString(PyExc_ValueError, err.msg);
    return -1;
  }
  self->i_doc->str_pool = buf;
  return 0;

error:
  Py_XDECREF(readinto);
  Py_XDECREF(readm);
  if (buf) self->alc->free(self->alc->ctx, buf);
  return -1;
}

/**
 * Parse JSON text from `content` into a new immutable document, dispatching on
 * its type: ``str``, ``bytes``, ``bytearray``, or a ``pathlib.Path`` (read from
 * disk). Shared by the Document constructor and the module-level ``loads``.
 *
 * On success returns the document. On failure returns NULL and sets `*not_text`:
 *   1  `content` was not one of the text types above; no exception is set, and
 *      the caller decides what to do (stream a file-like, build from a value,
 *      or raise).
 *   0  a read or parse error occurred and a Python exception is already set.
 */
static yyjson_doc *parse_content(
    PyObject *content, yyjson_read_flag flag, const yyjson_alc *alc,
    int *not_text
) {
  yyjson_read_err err;
  yyjson_doc *doc;

  *not_text = 0;

  if (yyjson_likely(PyBytes_Check(content))) {
    // Discarding const is safe as long as we never expose the insitu flag.
    doc = yyjson_read_opts((char *)PyBytes_AS_STRING(content),
                           (size_t)PyBytes_GET_SIZE(content), flag, alc, &err);
  } else if (yyjson_likely(PyUnicode_Check(content))) {
    Py_ssize_t len;
    const char *utf8 = PyUnicode_AsUTF8AndSize(content, &len);
    if (utf8 == NULL) return NULL;
    doc = yyjson_read_opts((char *)utf8, (size_t)len, flag, alc, &err);
  } else if (PyByteArray_Check(content)) {
    doc = yyjson_read_opts(PyByteArray_AS_STRING(content),
                           (size_t)PyByteArray_GET_SIZE(content), flag, alc,
                           &err);
  } else {
    int is_path;
    FILE *fp;
    is_path = PyObject_IsInstance(content, YY_PathClass);
    if (is_path < 0) return NULL;
    if (!is_path) {
      *not_text = 1;
      return NULL;
    }
    fp = fopen_path(content);
    if (fp == NULL) return NULL; /* OSError with the filename is set */
    doc = yyjson_read_fp(fp, flag, alc, &err);
    fclose(fp);
  }

  if (doc == NULL) {
    PyErr_SetString(PyExc_ValueError, err.msg);
  }
  return doc;
}

/**
 * Parse `content` as JSON text into self->i_doc. `content` may be a ``str``,
 * ``bytes``, ``bytearray``, a ``pathlib.Path`` (read from disk), or a binary
 * file-like object. Returns:
 *   0  parsed successfully,
 *  -1  an error occurred (a Python exception is set),
 *   1  `content` is not a JSON-text type and should be built from instead.
 */
static int document_read_json(
    DocumentObject *self, PyObject *content, yyjson_read_flag r_flag
) {
  int not_text;
  yyjson_doc *doc = parse_content(content, r_flag, self->alc, &not_text);

  if (doc != NULL) {
    self->i_doc = doc;
    return 0;
  }
  if (!not_text) {
    return -1;  // a read/parse error occurred; a Python exception is set
  }
  // A binary file-like object is streamed into a DOM; anything else is built
  // from as a Python value by the caller.
  if (PyObject_HasAttrString(content, "readinto") ||
      PyObject_HasAttrString(content, "read")) {
    return document_read_stream(self, content, r_flag);
  }
  return 1;
}

PyDoc_STRVAR(
    Document_init_doc,
    "A single JSON document.\n"
    "\n"
    "A `Document` can be built from a JSON-serializable Python object,\n"
    "a JSON document in a ``str``, a JSON document encoded to ``bytes``,\n"
    "or a ``Path()`` object to read a file from disk.\n"
    "Ex:\n"
    "\n"
    ".. doctest::\n"
    "\n"
    "   >>> Document({'a': 1, 'b': 2})\n"
    "   >>> Document(b'{\"a\": 1, \"b\": 2}')\n"
    "   >>> Document('{\"a\": 1, \"b\": 2}')\n"
    "   >>> Document(Path('path/to/file.json'))\n"
    "\n"
    "By default, the parsing is strict and follows the JSON specifications.\n"
    "You can change this behaviour by passing in :class:`ReaderFlags`. Ex:\n"
    "\n"
    ".. doctest::\n"
    "\n"
    "   >>> Document('''{\n"
    "   ...     // Comments in JSON!?!?\n"
    "   ...     \"a\": 1\n"
    "   ... }''', flags=ReaderFlags.ALLOW_COMMENTS)\n"
    "\n"
    ".. note::\n"
    "\n"
    "   yyjson has distinct APIs and data structures for mutable and "
    "immutable\n"
    "   documents. This class is a wrapper around both of them, and will\n"
    "   automatically convert between them as needed.\n"
    "\n"
    ":param content: The initial content of the document.\n"
    ":type content: ``str``, ``bytes``, ``Path``, ``dict``, ``list``\n"
    ":param flags: Flags that modify the document parsing behaviour.\n"
    ":type flags: :class:`ReaderFlags`, optional\n"
    ":param default: A function called to convert objects that are not\n"
    "                JSON serializable. Should return a JSON serializable version\n"
    "                of the object or raise a TypeError.\n"
    ":type default: callable, optional"
);
static int Document_init(DocumentObject *self, PyObject *args, PyObject *kwds) {
  static char *kwlist[] = {"content", "flags", "default", NULL};
  PyObject *content;
  PyObject *default_func = NULL;
  yyjson_read_flag r_flag = 0;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "O|$IO", kwlist, &content, &r_flag, &default_func
      )) {
    return -1;
  }

  if (default_func && default_func != Py_None && !PyCallable_Check(default_func)) {
    PyErr_SetString(PyExc_TypeError, "default must be callable");
    return -1;
  }

  if (self->i_doc) {
    yyjson_doc_free(self->i_doc);
    self->i_doc = NULL;
  }
  if (self->m_doc) {
    yyjson_mut_doc_free(self->m_doc);
    self->m_doc = NULL;
  }
  Py_CLEAR(self->default_func);

  self->default_func = default_func == Py_None ? NULL : default_func;
  Py_XINCREF(self->default_func);

  // A str/bytes/Path is parsed as JSON; anything else is built from as a
  // Python object.
  int result = document_read_json(self, content, r_flag);
  if (result == 1) {
    return document_build_from_object(self, content);
  }

  return result;
}

PyDoc_STRVAR(
    Document_from_obj_doc,
    "from_obj(obj, *, default=None)\n"
    "\n"
    "Build a :class:`Document` from a JSON-serializable Python object.\n"
    "\n"
    "Unlike the constructor, a ``str`` or ``bytes`` argument is serialized as a\n"
    "JSON value rather than parsed as JSON text.\n"
    "\n"
    ":param obj: The Python object to build the document from.\n"
    ":param default: A function called to convert objects that are not\n"
    "                JSON serializable.\n"
    ":type default: callable, optional"
);
static PyObject *Document_from_obj(
    PyObject *cls, PyObject *args, PyObject *kwds
) {
  static char *kwlist[] = {"obj", "default", NULL};
  PyObject *content = NULL;
  PyObject *default_func = NULL;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "O|$O", kwlist, &content, &default_func
      )) {
    return NULL;
  }

  if (default_func && default_func != Py_None &&
      !PyCallable_Check(default_func)) {
    PyErr_SetString(PyExc_TypeError, "default must be callable");
    return NULL;
  }

  PyTypeObject *type = (PyTypeObject *)cls;
  DocumentObject *self = (DocumentObject *)type->tp_alloc(type, 0);
  if (self == NULL) {
    return NULL;
  }

  self->m_doc = NULL;
  self->i_doc = NULL;
  self->alc = &PyMem_Allocator;
  self->default_func = default_func == Py_None ? NULL : default_func;
  Py_XINCREF(self->default_func);

  if (document_build_from_object(self, content) < 0) {
    Py_DECREF(self);
    return NULL;
  }

  return (PyObject *)self;
}

PyDoc_STRVAR(
    Document_from_json_doc,
    "from_json(content, *, flags=0)\n"
    "\n"
    "Parse a JSON document from a ``str``, ``bytes``, or a ``pathlib.Path``\n"
    "(read from disk).\n"
    "\n"
    "This is the explicit counterpart to :meth:`from_obj`: the argument is\n"
    "always parsed as JSON text, never built from as a Python value.\n"
    "\n"
    ":param content: The JSON document as ``str``/``bytes``, or a ``Path`` to a\n"
    "                file to read.\n"
    ":param flags: Flags that modify the parsing behaviour.\n"
    ":type flags: :class:`ReaderFlags`, optional"
);
static PyObject *Document_from_json(
    PyObject *cls, PyObject *args, PyObject *kwds
) {
  static char *kwlist[] = {"content", "flags", NULL};
  PyObject *content = NULL;
  yyjson_read_flag r_flag = 0;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "O|$I", kwlist, &content, &r_flag
      )) {
    return NULL;
  }

  PyTypeObject *type = (PyTypeObject *)cls;
  DocumentObject *self = (DocumentObject *)type->tp_alloc(type, 0);
  if (self == NULL) {
    return NULL;
  }

  self->m_doc = NULL;
  self->i_doc = NULL;
  self->alc = &PyMem_Allocator;
  self->default_func = NULL;

  int result = document_read_json(self, content, r_flag);
  if (result == 1) {
    PyErr_Format(PyExc_TypeError,
        "from_json() expects str, bytes, bytearray, or Path, not '%s'",
        Py_TYPE(content)->tp_name
    );
    result = -1;
  }
  if (result < 0) {
    Py_DECREF(self);
    return NULL;
  }

  return (PyObject *)self;
}

/*
 * Conversions run with the cyclic GC paused: the result is an acyclic tree,
 * so a collection mid-build can never free anything and only wastes time.
 * A depth counter makes pause/resume safe against overlap: conversions can
 * nest or interleave (a Decimal() call inside one can yield the GIL and run
 * another thread's conversion), so only the outermost pause snapshots and
 * toggles the GC state, and it is restored -- not force-enabled -- when the
 * last conversion finishes.
 */
#if !defined(PYPY_VERSION) && PY_VERSION_HEX >= 0x030A0000
static int gc_pause_depth = 0;
static int gc_we_disabled = 0;

static void gc_pause(void) {
  if (gc_pause_depth++ == 0) {
    gc_we_disabled = PyGC_IsEnabled();
    if (gc_we_disabled) PyGC_Disable();
  }
}

static void gc_resume(void) {
  if (--gc_pause_depth == 0 && gc_we_disabled) {
    PyGC_Enable();
  }
}
#else
#define gc_pause() ((void)0)
#define gc_resume() ((void)0)
#endif

/**
 * Convert a document's root to Python objects with the cyclic GC paused for
 * the duration (see gc_pause above).
 */
static PyObject *doc_root_to_obj(DocumentObject *self) {
  PyObject *result;
  gc_pause();
  if (self->i_doc) {
    result = element_to_primitive(yyjson_doc_get_root(self->i_doc), 0);
  } else {
    result = mut_element_to_primitive(yyjson_mut_doc_get_root(self->m_doc), 0);
  }
  gc_resume();
  return result;
}

/**
 * Recursively convert the document into Python objects.
 */
static PyObject *Document_as_obj(DocumentObject *self, void *closure) {
  return doc_root_to_obj(self);
}

PyDoc_STRVAR(
    py_loads_doc,
    "loads(s)\n"
    "\n"
    "Parse a JSON document from a ``str``, ``bytes``, ``bytearray``, or a\n"
    "``pathlib.Path`` (read from disk) and return the equivalent Python object.");
static PyObject *py_loads(PyObject *module, PyObject *arg) {
  int not_text;
  yyjson_doc *doc;
  PyObject *result;
  (void)module;

  doc = parse_content(arg, 0, &PyMem_Allocator, &not_text);
  if (doc == NULL) {
    if (not_text) {
      PyErr_Format(
          PyExc_TypeError,
          "loads() argument must be str, bytes, bytearray, or Path, not '%s'",
          Py_TYPE(arg)->tp_name);
    }
    return NULL;
  }

  /* Convert with the cyclic GC paused (acyclic tree; see gc_pause). */
  gc_pause();
  result = element_to_primitive(yyjson_doc_get_root(doc), 0);
  gc_resume();

  yyjson_doc_free(doc);
  return result;
}

PyMethodDef yyjson_doc_methods[] = {
    {"loads", (PyCFunction)py_loads, METH_O, py_loads_doc},
    {NULL} /* Sentinel */
};

/**
 * Is the document mutable?
 */
static PyObject *Document_is_thawed(DocumentObject *self, void *closure) {
  return PyBool_FromLong(self->m_doc != NULL);
}

/**
 * Get the size of data read from the original JSON input.
 */
static PyObject *Document_bytes_read(DocumentObject *self, void *closure) {
  if (self->i_doc) {
    return PyLong_FromSize_t(yyjson_doc_get_read_size(self->i_doc));
  } else {
    return PyLong_FromLong(0);
  }
}

PyDoc_STRVAR(
    Document_dumps_doc,
    "Dumps the document to a string and returns it.\n"
    "\n"
    "By default, serializes to a minified string and strictly follows the\n"
    "JSON specification. Ex:\n"
    "\n"
    ".. doctest::\n"
    "\n"
    "    >>> doc = Document({'hello': 'world'})\n"
    "    >>> print(doc.dumps())\n"
    "    {\"hello\":\"world\"}\n"
    "\n"
    "This behaviour can be controlled by passing :class:`WriterFlags`. Ex:\n"
    "\n"
    ".. doctest::\n"
    "\n"
    "    >>> doc = Document({'hello': 'world'})\n"
    "    >>> print(doc.dumps(flags=WriterFlags.PRETTY))\n"
    "    {\n"
    "        \"hello\": \"world\"\n"
    "    }\n"
    "\n"
    "To dump just part of a document, you can pass a JSON pointer (RFC 6901)\n"
    "as ``at_pointer``, ex:\n"
    "\n"
    ".. doctest::\n"
    "\n"
    "    >>> doc = Document({'results': {'count': 3, 'rows': [55, 66, 77]}})\n"
    "    >>> print(doc.dumps(at_pointer='/results/rows'))\n"
    "    [55,66,77]\n"
    "\n"
    ":param flags: Flags that control JSON writing behaviour.\n"
    ":type flags: :class:`yyjson.WriterFlags`, optional\n"
    ":param at_pointer: An optional JSON pointer specifying what part of the\n"
    "                   document should be dumped. If not specified, defaults\n"
    "                   to the entire ``Document``.\n"
    ":type at_pointer: str, optional\n"
    ":returns: The serialized ``Document``.\n"
    ":rtype: ``str``"
);
static PyObject *Document_dumps(
    DocumentObject *self, PyObject *args, PyObject *kwds
) {
  static char *kwlist[] = {"flags", "at_pointer", NULL};
  yyjson_write_flag w_flag = 0;
  const char *pointer = NULL;
  Py_ssize_t pointer_size;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "|$Is#", kwlist, &w_flag, &pointer, &pointer_size
      )) {
    return NULL;
  }

  char *result = NULL;
  size_t w_len;
  yyjson_write_err w_err;
  PyObject *obj_result = NULL;

  if (self->i_doc) {
    yyjson_val *val_to_serialize = NULL;

    if (pointer) {
      val_to_serialize =
          yyjson_doc_ptr_getn(self->i_doc, pointer, pointer_size);
    } else {
      val_to_serialize = yyjson_doc_get_root(self->i_doc);
    }

    result = yyjson_val_write_opts(
        val_to_serialize, w_flag, self->alc, &w_len, &w_err
    );
  } else {
    yyjson_mut_val *mut_val_to_serialize = NULL;

    if (pointer) {
      mut_val_to_serialize =
          yyjson_mut_doc_ptr_getn(self->m_doc, pointer, pointer_size);
    } else {
      mut_val_to_serialize = yyjson_mut_doc_get_root(self->m_doc);
    }

    result = yyjson_mut_val_write_opts(
        mut_val_to_serialize, w_flag, self->alc, &w_len, &w_err
    );
  }

  if (yyjson_unlikely(!result)) {
    PyErr_SetString(PyExc_ValueError, w_err.msg);
    return NULL;
  }

  obj_result = PyUnicode_FromStringAndSize(result, w_len);
  self->alc->free(NULL, result);

  return obj_result;
}

PyDoc_STRVAR(
    Document_get_pointer_doc,
    "Returns the JSON element at the given JSON pointer (RFC 6901).\n"
    "\n"
    ":param pointer: JSON Pointer to search for.\n"
    ":type pointer: ``str``"
);
static PyObject *Document_get_pointer(DocumentObject *self, PyObject *args) {
  char *pointer = NULL;
  Py_ssize_t pointer_len;

  if (!PyArg_ParseTuple(args, "s#", &pointer, &pointer_len)) {
    return NULL;
  }

  yyjson_ptr_err err;

  if (self->i_doc) {
    yyjson_val *result =
        yyjson_doc_ptr_getx(self->i_doc, pointer, pointer_len, &err);

    if (!result) {
      PyErr_SetString(
          PyExc_ValueError, err.msg ? err.msg : "Not a valid JSON Pointer"
      );
      return NULL;
    }

    return element_to_primitive(result, 0);
  } else {
    yyjson_mut_val *result =
        yyjson_mut_doc_ptr_getx(self->m_doc, pointer, pointer_len, NULL, &err);

    if (!result) {
      PyErr_SetString(
          PyExc_ValueError, err.msg ? err.msg : "Not a valid JSON Pointer"
      );
      return NULL;
    }

    return mut_element_to_primitive(result, 0);
  }
}

PyDoc_STRVAR(
    Document_freeze_doc,
    "Freezes the document, copying it into yyjson's read-only internal "
    "object.\n"
    "\n"
    "This object can be used as a normal ``Document`` object, but uses less\n"
    "memory after creation, and offers slightly improved performance.\n"
    "\n"
    ".. note::\n"
    "\n"
    "    If a ``Document`` method that requires mutation is called on a "
    "frozen\n"
    "    ``Document``, such as :func:`patch()`, it will be automatically "
    "thawed.\n"
    "    This is an advanced function and can usually be ignored.\n"
);
static PyObject *Document_freeze(DocumentObject *self) {
  if (self->m_doc) {
    self->i_doc = yyjson_mut_doc_imut_copy(self->m_doc, self->alc);
    yyjson_mut_doc_free(self->m_doc);
    self->m_doc = NULL;
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    Document_thaw_doc,
    "Thaws the document, copying it into yyjson's mutable internal object.\n"
    "\n"
    "This object can be used as a normal ``Document`` object, but will use\n"
    "slightly more memory after creation, and offers slightly worse\n"
    "performance.\n"
    "\n"
    ".. note::\n"
    "\n"
    "    This is an advanced function and can usually be ignored.\n"
);
static PyObject *Document_thaw(DocumentObject *self) {
  if (self->i_doc) {
    self->m_doc = yyjson_doc_mut_copy(self->i_doc, self->alc);
    yyjson_doc_free(self->i_doc);
    self->i_doc = NULL;
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    Document_patch_doc,
    "Patch a ``Document`` with another ``Document``, using either JSON Patch "
    "(RFC 6902)\n"
    "or JSON Merge-Patch (RFC 7386).\n"
    "\n"
    "By default, this will apply a JSON Patch. Specify "
    "``use_merge_patch=True`` to\n"
    "use JSON Merge-Patch instead.\n"
    "\n"
    ":param patch: The ``Document`` to patch with.\n"
    ":type patch: ``Document``\n"
    ":param at_pointer: The (optional) JSON Pointer (RFC 6901) to patch at,\n"
    "                   instead of patching the entire document.\n"
    ":type at_pointer: ``str``\n"
    ":param use_merge_patch: Whether to use JSON Merge-Patch (RFC 7386) "
    "instead of\n"
    "    JSON Patch (RFC 6902).\n"
    "\n"
);
static PyObject *Document_patch(
    DocumentObject *self, PyObject *args, PyObject *kwds
) {
  static char *kwlist[] = {"patch", "at_pointer", "use_merge_patch", NULL};

  const char *pointer = NULL;
  Py_ssize_t pointer_size;
  PyObject *patch = NULL;
  int use_merge_patch = false;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds,
          /* We can switch the "i" for "p" to be explicit with our bool
           * flag, but only after we drop support for everything < 3.3. */
          "O|$z#i", kwlist, &patch, &pointer, &pointer_size, &use_merge_patch
      )) {
    return NULL;
  }

  // Accept a Document, or coerce any Document-constructible value (a dict,
  // list, JSON str/bytes, or Path) into a temporary one, so callers don't have
  // to wrap patches by hand. The mutable and immutable paths below both cast
  // `patch` to a DocumentObject and dereference it, so it must be one.
  // `patch_owned` is non-NULL only when we created the temporary and must free
  // it before returning.
  PyObject *patch_owned = NULL;
  if (!PyObject_IsInstance(patch, (PyObject *)&DocumentType)) {
    patch = PyObject_CallFunction((PyObject *)&DocumentType, "(O)", patch);
    if (!patch) {
      return NULL;
    }
    patch_owned = patch;
  }

  // Create a new, essentially empty Document which will serve as the
  // container for the patch result.
  DocumentObject *obj = (DocumentObject *)PyObject_CallFunction(
      (PyObject *)&DocumentType, "(O)", Py_None
  );
  if (!obj) {
    PyErr_SetString(
        PyExc_ValueError,
        "Unable to create container Document for results of merge-patch"
    );
    Py_XDECREF(patch_owned);
    return NULL;
  }

  DocumentObject *patch_doc = (DocumentObject *)patch;
  yyjson_doc *tmp_patch = NULL;

  // If a pointer was provided, that's the value we're going to be patching,
  // otherwise we use the root of the document.
  if (self->i_doc) {
    yyjson_val *original = NULL;

    if (pointer != NULL) {
      yyjson_ptr_err ptr_err;

      original =
          yyjson_doc_ptr_getx(self->i_doc, pointer, pointer_size, &ptr_err);
      if (!original) {
        PyErr_SetString(
            PyExc_ValueError,
            ptr_err.msg ? ptr_err.msg : "Not a valid JSON Pointer"
        );
        goto error;
      }
    } else {
      original = yyjson_doc_get_root(self->i_doc);
      if (yyjson_unlikely(!original)) {
        PyErr_SetString(PyExc_ValueError, "Document has no root.");
        goto error;
      }
    }

    yyjson_val *patch_val = NULL;
    if (patch_doc->i_doc) {
      patch_val = yyjson_doc_get_root(patch_doc->i_doc);
    } else if (patch_doc->m_doc) {
      tmp_patch = yyjson_mut_doc_imut_copy(patch_doc->m_doc, self->alc);
      if (!tmp_patch) {
        PyErr_NoMemory();
        goto error;
      }
      patch_val = yyjson_doc_get_root(tmp_patch);
    }
    if (!patch_val) {
      PyErr_SetString(PyExc_ValueError, "Patch document has no root value.");
      goto error;
    }

    yyjson_mut_val *patched_val = NULL;

    if (use_merge_patch) {
      patched_val = yyjson_merge_patch(obj->m_doc, original, patch_val);
    } else {
      yyjson_patch_err patch_err;

      patched_val = yyjson_patch(obj->m_doc, original, patch_val, &patch_err);

      if (!patched_val) {
        PyErr_SetString(
            PyExc_ValueError,
            patch_err.msg ? patch_err.msg : "Unable to apply patch to document."
        );
        goto error;
      }
    }

    if (!patched_val) {
      PyErr_SetString(PyExc_ValueError, "Unable to apply patch to document.");
      goto error;
    }

    yyjson_mut_doc_set_root(obj->m_doc, patched_val);
  } else {
    yyjson_mut_val *original = NULL;

    if (pointer != NULL) {
      yyjson_ptr_err ptr_err;

      original = yyjson_mut_doc_ptr_getx(
          self->m_doc, pointer, pointer_size, NULL, &ptr_err
      );
      if (!original) {
        PyErr_SetString(
            PyExc_ValueError,
            ptr_err.msg ? ptr_err.msg : "Not a valid JSON Pointer"
        );
        goto error;
      }
    } else {
      original = yyjson_mut_doc_get_root(self->m_doc);
      if (yyjson_unlikely(!original)) {
        PyErr_SetString(PyExc_ValueError, "Document has no root.");
        goto error;
      }
    }

    yyjson_mut_val *patch_val = NULL;
    if (patch_doc->m_doc) {
      patch_val = yyjson_mut_doc_get_root(patch_doc->m_doc);
    } else if (patch_doc->i_doc) {
      yyjson_val *iroot = yyjson_doc_get_root(patch_doc->i_doc);
      if (iroot) {
        patch_val = yyjson_val_mut_copy(obj->m_doc, iroot);
        if (!patch_val) {
          PyErr_NoMemory();
          goto error;
        }
      }
    }
    if (!patch_val) {
      PyErr_SetString(PyExc_ValueError, "Patch document has no root value.");
      goto error;
    }

    yyjson_mut_val *patched_val;

    if (use_merge_patch) {
      patched_val = yyjson_mut_merge_patch(obj->m_doc, original, patch_val);
    } else {
      yyjson_patch_err patch_err;

      patched_val =
          yyjson_mut_patch(obj->m_doc, original, patch_val, &patch_err);

      if (!patched_val) {
        PyErr_SetString(
            PyExc_ValueError,
            patch_err.msg ? patch_err.msg : "Unable to apply patch to document."
        );
        goto error;
      }
    }

    if (!patched_val) {
      PyErr_SetString(PyExc_ValueError, "Unable to apply patch to document.");
      goto error;
    }

    yyjson_mut_doc_set_root(obj->m_doc, patched_val);
  }

  Py_XDECREF(patch_owned);
  if (tmp_patch) yyjson_doc_free(tmp_patch);
  return (PyObject *)obj;

error:
  Py_XDECREF(patch_owned);
  if (tmp_patch) yyjson_doc_free(tmp_patch);
  Py_DECREF(obj);
  return NULL;
}

static Py_ssize_t Document_length(DocumentObject *self) {
  if (self->i_doc) {
    return yyjson_get_len(yyjson_doc_get_root(self->i_doc));
  } else {
    return yyjson_mut_get_len(yyjson_mut_doc_get_root(self->m_doc));
  }
}

static PyMethodDef Document_methods[] = {
    {"from_obj", (PyCFunction)(void (*)(void))Document_from_obj,
     METH_VARARGS | METH_KEYWORDS | METH_CLASS, Document_from_obj_doc},
    {"from_json", (PyCFunction)(void (*)(void))Document_from_json,
     METH_VARARGS | METH_KEYWORDS | METH_CLASS, Document_from_json_doc},
    {"patch", (PyCFunction)(void (*)(void))Document_patch,
     METH_VARARGS | METH_KEYWORDS, Document_patch_doc},
    {"dumps", (PyCFunction)(void (*)(void))Document_dumps,
     METH_VARARGS | METH_KEYWORDS, Document_dumps_doc},
    {"get_pointer", (PyCFunction)(void (*)(void))Document_get_pointer,
     METH_VARARGS, Document_get_pointer_doc},
    {"freeze", (PyCFunction)(void (*)(void))Document_freeze, METH_NOARGS,
     Document_freeze_doc},
    {"thaw", (PyCFunction)(void (*)(void))Document_thaw, METH_NOARGS,
     Document_thaw_doc},
    {NULL} /* Sentinel */
};

static PyGetSetDef Document_members[] = {
    {"as_obj", (getter)Document_as_obj, NULL,
     "Converts the Document to a native Python object, such as a ``dict`` or "
     "``list``.",
     NULL},
    {"is_thawed", (getter)Document_is_thawed, NULL,
     "Returns whether the Document is thawed/mutable.", NULL},
    {"bytes_read", (getter)Document_bytes_read, NULL,
     "Returns the size of data read from the original JSON input.", NULL},
    {NULL} /* Sentinel */
};

static PyMappingMethods Document_mapping_methods = {
    (lenfunc)Document_length, NULL, NULL};

PyTypeObject DocumentType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "cyyjson.Document",
    .tp_doc = Document_init_doc,
    .tp_basicsize = sizeof(DocumentObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Document_new,
    .tp_init = (initproc)Document_init,
    .tp_dealloc = (destructor)Document_dealloc,
    .tp_getset = Document_members,
    .tp_as_mapping = &Document_mapping_methods,
    .tp_methods = Document_methods};
