#ifndef PY_YYJSON_MUTABLE_H
#define PY_YYJSON_MUTABLE_H

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "yyjson.h"

/**
 * Represents a mutable yyjson document.
 */
typedef struct {
    PyObject_HEAD
    /** A mutable parsed document. */
    yyjson_mut_doc *m_doc;
    /** The memory allocator in use for this document. */
    yyjson_alc *alc;
} MutableDocumentObject;

extern PyTypeObject MutableDocumentType;

#endif
