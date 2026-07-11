#ifndef PY_YYJSON_DOCUMENT_H
#define PY_YYJSON_DOCUMENT_H

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "yyjson.h"

/**
 * Represents a single yyjson document.
 */
typedef struct {
  PyObject_HEAD
      /** A mutable document. */
      yyjson_mut_doc* m_doc;
  /** An immutable document. */
  yyjson_doc* i_doc;
  /** The memory allocator in use for this document. */
  yyjson_alc* alc;
  /** default callback for serializing unknown types. */
  PyObject* default_func;
} DocumentObject;

extern PyTypeObject DocumentType;

/**
 * Convert a UTF-8 string of `len` bytes into a Python ``str``, using a fast
 * path for pure-ASCII input. Shared with the streaming (SAX) reader.
 */
PyObject *unicode_from_str(const char *src, size_t len);

#endif