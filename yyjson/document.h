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
} DocumentObject;

extern PyTypeObject DocumentType;

#endif