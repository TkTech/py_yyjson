#ifndef PY_YYJSON_IMMUTABLE_H
#define PY_YYJSON_IMMUTABLE_H

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "yyjson.h"

/**
 * Represents an immutable yyjson document.
 */
typedef struct {
    PyObject_HEAD
    /** An immutable parsed document. */
    yyjson_doc *i_doc;
    /** The memory allocator in use for this document. */
    yyjson_alc *alc;
} DocumentObject;

typedef struct {
    PyObject_HEAD
    DocumentObject *doc;
} MappingObject;

extern PyTypeObject DocumentType;

#endif