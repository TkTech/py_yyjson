#include "sax.h"

#include "document.h"
#include "memory.h"
#include "decimal.h"
#include "yyjson.h"

#include <stdio.h>

/*==============================================================================
 * Streaming (SAX) reader binding.
 *
 * Wraps `yyjson_sax_read()` so a Python `handler` object receives a method call
 * per JSON token, with peak memory bounded by a sliding window rather than the
 * document size. The GIL is held throughout (the source and handler callbacks
 * re-enter Python), so no threading juggling is required.
 *============================================================================*/

/* Bound handler methods (borrowed slots owned by this struct) plus flags used
   to propagate a Python exception or a clean early stop out of the C reader. */
typedef struct {
    PyObject *m_obj_begin;
    PyObject *m_obj_end;
    PyObject *m_arr_begin;
    PyObject *m_arr_end;
    PyObject *m_key;
    PyObject *m_string;
    PyObject *m_number;
    PyObject *m_boolean;
    PyObject *m_null;
    bool error;   /* a Python exception was raised inside a callback/source */
    bool stopped; /* a handler returned False, requesting a clean early stop */
} py_sax_ctx;

/* Fetch an optional, callable handler method. Returns a new reference, or NULL
   if the attribute is missing, None, or not callable (event then ignored). */
static PyObject *get_method(PyObject *handler, const char *name) {
    PyObject *m = PyObject_GetAttrString(handler, name);
    if (!m) {
        PyErr_Clear();
        return NULL;
    }
    if (m == Py_None || !PyCallable_Check(m)) {
        Py_DECREF(m);
        return NULL;
    }
    return m;
}

static void free_methods(py_sax_ctx *c) {
    Py_XDECREF(c->m_obj_begin);
    Py_XDECREF(c->m_obj_end);
    Py_XDECREF(c->m_arr_begin);
    Py_XDECREF(c->m_arr_end);
    Py_XDECREF(c->m_key);
    Py_XDECREF(c->m_string);
    Py_XDECREF(c->m_number);
    Py_XDECREF(c->m_boolean);
    Py_XDECREF(c->m_null);
}

/* Consume the result of a handler call: NULL -> exception (abort), Py_False ->
   clean stop (abort), anything else -> continue. */
static bool sax_finish(py_sax_ctx *c, PyObject *result) {
    bool cont;
    if (!result) {
        c->error = true;
        return false;
    }
    cont = (result != Py_False);
    Py_DECREF(result);
    if (!cont) c->stopped = true;
    return cont;
}

/* Convert a scalar number value to a Python object. RAW (Decimal) must be
   checked by type first: YYJSON_SUBTYPE_UINT == 0 collides with RAW's subtype. */
static PyObject *sax_num_to_py(const yyjson_val *v) {
    if (yyjson_get_type(v) == YYJSON_TYPE_RAW) {
        PyObject *uni = unicode_from_str(yyjson_get_raw(v), yyjson_get_len(v));
        PyObject *res;
        if (!uni) return NULL;
        res = PyObject_CallOneArg(YY_DecimalClass, uni);
        Py_DECREF(uni);
        return res;
    }
    switch (yyjson_get_subtype(v)) {
    case YYJSON_SUBTYPE_UINT:
        return PyLong_FromUnsignedLongLong(yyjson_get_uint(v));
    case YYJSON_SUBTYPE_SINT:
        return PyLong_FromLongLong(yyjson_get_sint(v));
    default: /* REAL */
        return PyFloat_FromDouble(yyjson_get_real(v));
    }
}

/*------------------------------------------------------------------------------
 * Handler trampolines (installed only for methods the handler defines).
 *----------------------------------------------------------------------------*/

static bool tr_obj_begin(void *ctx) {
    py_sax_ctx *c = (py_sax_ctx *)ctx;
    return sax_finish(c, PyObject_CallNoArgs(c->m_obj_begin));
}
static bool tr_arr_begin(void *ctx) {
    py_sax_ctx *c = (py_sax_ctx *)ctx;
    return sax_finish(c, PyObject_CallNoArgs(c->m_arr_begin));
}
static bool tr_null(void *ctx) {
    py_sax_ctx *c = (py_sax_ctx *)ctx;
    return sax_finish(c, PyObject_CallNoArgs(c->m_null));
}
static bool tr_obj_end(void *ctx, size_t n) {
    py_sax_ctx *c = (py_sax_ctx *)ctx;
    PyObject *a = PyLong_FromSize_t(n), *r;
    if (!a) { c->error = true; return false; }
    r = PyObject_CallOneArg(c->m_obj_end, a);
    Py_DECREF(a);
    return sax_finish(c, r);
}
static bool tr_arr_end(void *ctx, size_t n) {
    py_sax_ctx *c = (py_sax_ctx *)ctx;
    PyObject *a = PyLong_FromSize_t(n), *r;
    if (!a) { c->error = true; return false; }
    r = PyObject_CallOneArg(c->m_arr_end, a);
    Py_DECREF(a);
    return sax_finish(c, r);
}
static bool tr_key(void *ctx, const char *s, size_t n) {
    py_sax_ctx *c = (py_sax_ctx *)ctx;
    PyObject *a = unicode_from_str(s, n), *r;
    if (!a) { c->error = true; return false; }
    r = PyObject_CallOneArg(c->m_key, a);
    Py_DECREF(a);
    return sax_finish(c, r);
}
static bool tr_string(void *ctx, const char *s, size_t n) {
    py_sax_ctx *c = (py_sax_ctx *)ctx;
    PyObject *a = unicode_from_str(s, n), *r;
    if (!a) { c->error = true; return false; }
    r = PyObject_CallOneArg(c->m_string, a);
    Py_DECREF(a);
    return sax_finish(c, r);
}
static bool tr_number(void *ctx, const yyjson_val *v) {
    py_sax_ctx *c = (py_sax_ctx *)ctx;
    PyObject *a = sax_num_to_py(v), *r;
    if (!a) { c->error = true; return false; }
    r = PyObject_CallOneArg(c->m_number, a);
    Py_DECREF(a);
    return sax_finish(c, r);
}
static bool tr_boolean(void *ctx, bool v) {
    py_sax_ctx *c = (py_sax_ctx *)ctx;
    return sax_finish(c, PyObject_CallOneArg(c->m_boolean,
                                             v ? Py_True : Py_False));
}

/*------------------------------------------------------------------------------
 * Byte sources.
 *----------------------------------------------------------------------------*/

/* A source over an already-materialized in-memory buffer (bytes/str). */
typedef struct {
    const char *dat;
    size_t len;
    size_t pos;
} mem_src;

static size_t mem_source(void *ctx, void *buf, size_t len) {
    mem_src *s = (mem_src *)ctx;
    size_t give = s->len - s->pos;
    if (give > len) give = len;
    memcpy(buf, s->dat + s->pos, give);
    s->pos += give;
    return give;
}

/* A source over a Python binary file-like object. Prefers ``readinto`` (fills
   the window directly, no intermediate allocation) and falls back to ``read``. */
typedef struct {
    PyObject *readinto; /* borrowed */
    PyObject *read;     /* borrowed */
    py_sax_ctx *pc;
} py_src;

static size_t py_source(void *ctx, void *buf, size_t len) {
    py_src *s = (py_src *)ctx;

    if (s->readinto) {
        PyObject *mv, *r;
        Py_ssize_t n;
        mv = PyMemoryView_FromMemory((char *)buf, (Py_ssize_t)len, PyBUF_WRITE);
        if (!mv) { s->pc->error = true; return YYJSON_SAX_SOURCE_ERROR; }
        r = PyObject_CallOneArg(s->readinto, mv);
        Py_DECREF(mv);
        if (!r) { s->pc->error = true; return YYJSON_SAX_SOURCE_ERROR; }
        if (r == Py_None) {
            /* None means "no data available right now" on a non-blocking
               stream, not EOF; treating it as EOF would silently truncate. */
            Py_DECREF(r);
            PyErr_SetString(PyExc_BlockingIOError,
                            "readinto() returned None (no data available on "
                            "a non-blocking stream); a blocking stream is "
                            "required");
            s->pc->error = true;
            return YYJSON_SAX_SOURCE_ERROR;
        }
        n = PyNumber_AsSsize_t(r, NULL);
        Py_DECREF(r);
        if (n < 0) {
            if (!PyErr_Occurred())
                PyErr_SetString(PyExc_ValueError, "readinto() returned < 0");
            s->pc->error = true;
            return YYJSON_SAX_SOURCE_ERROR;
        }
        return (size_t)n;
    } else {
        PyObject *r;
        char *data;
        Py_ssize_t n;
        r = PyObject_CallFunction(s->read, "n", (Py_ssize_t)len);
        if (!r) { s->pc->error = true; return YYJSON_SAX_SOURCE_ERROR; }
        if (r == Py_None) {
            Py_DECREF(r);
            PyErr_SetString(PyExc_BlockingIOError,
                            "read() returned None (no data available on a "
                            "non-blocking stream); a blocking stream is "
                            "required");
            s->pc->error = true;
            return YYJSON_SAX_SOURCE_ERROR;
        }
        if (!PyBytes_Check(r)) {
            Py_DECREF(r);
            PyErr_SetString(PyExc_TypeError,
                            "read() must return bytes; open the stream in "
                            "binary mode");
            s->pc->error = true;
            return YYJSON_SAX_SOURCE_ERROR;
        }
        if (PyBytes_AsStringAndSize(r, &data, &n) < 0) {
            Py_DECREF(r);
            s->pc->error = true;
            return YYJSON_SAX_SOURCE_ERROR;
        }
        if ((size_t)n > len) n = (Py_ssize_t)len; /* defensive */
        memcpy(buf, data, (size_t)n);
        Py_DECREF(r);
        return (size_t)n;
    }
}

/*------------------------------------------------------------------------------
 * Entry point.
 *----------------------------------------------------------------------------*/

PyDoc_STRVAR(
    py_sax_doc,
    "sax(source, handler, *, flags=0, window_size=0, max_depth=0)\n"
    "\n"
    "Parse JSON from ``source`` using bounded memory, calling methods on\n"
    "``handler`` for each token as it is encountered. Peak memory is bounded by\n"
    "a sliding window (``window_size``) plus the nesting depth, independent of\n"
    "the total input size, so inputs far larger than RAM can be processed.\n"
    "\n"
    "``source`` may be ``str``, any object exporting a contiguous byte buffer\n"
    "(``bytes``, ``bytearray``, ``memoryview``, ``mmap``, ...), a binary\n"
    "file-like object (with ``readinto`` or ``read``), or a ``pathlib.Path``\n"
    "to open and stream. Buffer sources are read zero-copy and are pinned for\n"
    "the duration of the parse: resizing one from a handler callback raises\n"
    "``BufferError``. File-like sources must be blocking: a read that returns\n"
    "``None`` raises ``BlockingIOError``.\n"
    "\n"
    "``handler`` is any object; the following methods are called if present\n"
    "(each is optional):\n"
    "\n"
    "* ``obj_begin()`` / ``obj_end(count)``\n"
    "* ``arr_begin()`` / ``arr_end(count)``\n"
    "* ``key(str)`` -- an object member key\n"
    "* ``string(str)`` -- a string value\n"
    "* ``number(value)`` -- an ``int``, ``float``, or ``Decimal`` value\n"
    "* ``boolean(bool)``\n"
    "* ``null()``\n"
    "\n"
    "A handler method may return ``False`` to stop parsing early (``sax()``\n"
    "then returns normally). Raising propagates the exception.\n"
    "\n"
    "The one limitation is that a single string or number token may not exceed\n"
    "``window_size``; such input raises ``ValueError``. Only standard JSON is\n"
    "supported; ``NUMBERS_AS_DECIMAL``/``BIGNUM_AS_RAW`` and ``STOP_WHEN_DONE``\n"
    "flags are honored, others are ignored.\n"
    "\n"
    ":param source: The JSON input.\n"
    ":param handler: An object receiving a method call per token.\n"
    ":param flags: :class:`ReaderFlags` controlling parsing.\n"
    ":param window_size: Sliding window size in bytes (0 = default, 256 KiB).\n"
    ":param max_depth: Maximum container nesting depth (0 = default)."
);
static PyObject *py_sax(PyObject *self, PyObject *args, PyObject *kwds) {
    static char *kwlist[] = {"source",     "handler",   "flags",
                             "window_size", "max_depth", NULL};
    PyObject *source = NULL, *handler = NULL;
    unsigned int flags = 0;
    Py_ssize_t window = 0, max_depth = 0;
    py_sax_ctx pc;
    yyjson_sax_handler h;
    yyjson_sax_opts opts;
    yyjson_read_err err;
    bool ok = false;
    int handled = 0; /* 0 = source type not yet consumed */

    (void)self;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO|$Inn", kwlist, &source,
                                     &handler, &flags, &window, &max_depth)) {
        return NULL;
    }

    memset(&pc, 0, sizeof(pc));
    pc.m_obj_begin = get_method(handler, "obj_begin");
    pc.m_obj_end = get_method(handler, "obj_end");
    pc.m_arr_begin = get_method(handler, "arr_begin");
    pc.m_arr_end = get_method(handler, "arr_end");
    pc.m_key = get_method(handler, "key");
    pc.m_string = get_method(handler, "string");
    pc.m_number = get_method(handler, "number");
    pc.m_boolean = get_method(handler, "boolean");
    pc.m_null = get_method(handler, "null");

    memset(&h, 0, sizeof(h));
    if (pc.m_obj_begin) h.obj_begin = tr_obj_begin;
    if (pc.m_obj_end) h.obj_end = tr_obj_end;
    if (pc.m_arr_begin) h.arr_begin = tr_arr_begin;
    if (pc.m_arr_end) h.arr_end = tr_arr_end;
    if (pc.m_key) h.key = tr_key;
    if (pc.m_string) h.str = tr_string;
    if (pc.m_number) h.num = tr_number;
    if (pc.m_boolean) h.bool_val = tr_boolean;
    if (pc.m_null) h.null_val = tr_null;

    memset(&opts, 0, sizeof(opts));
    if (window > 0) opts.window = (size_t)window;
    if (max_depth > 0) opts.max_depth = (size_t)max_depth;
    memset(&err, 0, sizeof(err));

    /* in-memory: str */
    if (PyUnicode_Check(source)) {
        mem_src ms;
        Py_ssize_t len = 0;
        ms.pos = 0;
        ms.dat = PyUnicode_AsUTF8AndSize(source, &len);
        if (!ms.dat) goto cleanup;
        ms.len = (size_t)len;
        ok = yyjson_sax_read(mem_source, &ms, &h, &pc, flags, &opts,
                             &PyMem_Allocator, &err);
        handled = 1;
    }

    /* in-memory: anything exporting a contiguous byte buffer (bytes,
       bytearray, memoryview, mmap, ...). Holding the buffer export for the
       whole parse pins the memory: a handler callback that tries to resize
       the source (e.g. a bytearray) gets a BufferError from Python instead
       of leaving our pointer dangling. */
    if (!handled && PyObject_CheckBuffer(source)) {
        Py_buffer view;
        mem_src ms;
        if (PyObject_GetBuffer(source, &view, PyBUF_SIMPLE) < 0) goto cleanup;
        ms.dat = (const char *)view.buf;
        ms.len = (size_t)view.len;
        ms.pos = 0;
        ok = yyjson_sax_read(mem_source, &ms, &h, &pc, flags, &opts,
                             &PyMem_Allocator, &err);
        PyBuffer_Release(&view);
        handled = 1;
    }

    /* pathlib.Path (or any os.PathLike, but not str/bytes handled above) */
    if (!handled && PyObject_HasAttrString(source, "__fspath__")) {
        FILE *fp = fopen_path(source);
        if (!fp) goto cleanup;
        ok = yyjson_sax_read_fp(fp, &h, &pc, flags, &opts,
                                &PyMem_Allocator, &err);
        fclose(fp);
        handled = 1;
    }

    /* binary file-like object */
    if (!handled) {
        py_src ps;
        ps.pc = &pc;
        ps.readinto = get_method(source, "readinto");
        ps.read = ps.readinto ? NULL : get_method(source, "read");
        if (!ps.readinto && !ps.read) {
            PyErr_Format(PyExc_TypeError,
                         "sax() source must be str, a bytes-like object, a "
                         "binary file-like object, or a Path, not '%s'",
                         Py_TYPE(source)->tp_name);
            goto cleanup;
        }
        ok = yyjson_sax_read(py_source, &ps, &h, &pc, flags, &opts,
                             &PyMem_Allocator, &err);
        Py_XDECREF(ps.readinto);
        Py_XDECREF(ps.read);
        handled = 1;
    }

cleanup:
    free_methods(&pc);

    if (pc.error) {
        /* a Python exception is already set by the callback/source */
        return NULL;
    }
    if (!handled) {
        /* an exception was set above before dispatching */
        return NULL;
    }
    if (!ok && err.code != YYJSON_READ_ERROR_ABORTED) {
        PyErr_Format(PyExc_ValueError, "%s (at byte %zu)",
                     err.msg ? err.msg : "SAX parse error", err.pos);
        return NULL;
    }
    Py_RETURN_NONE;
}

PyMethodDef yyjson_sax_methods[] = {
    {"sax", (PyCFunction)(void (*)(void))py_sax,
     METH_VARARGS | METH_KEYWORDS, py_sax_doc},
    {NULL} /* Sentinel */
};
