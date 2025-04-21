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
  /** numbers and bignums as decimal? */
  bool raw_as_decimal;
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

#endif
