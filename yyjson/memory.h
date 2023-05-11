#ifndef PY_YYJSON_MEMORY_H
#define PY_YYJSON_MEMORY_H

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "yyjson.h"

/** wrapper to use PyMem_Malloc with yyjson's allocator. **/
void* py_malloc(void* ctx, size_t size);

/** wrapper to use PyMem_Realloc with yyjson's allocator. **/
void* py_realloc(void* ctx, void* ptr, size_t old_size, size_t size);

/** wrapper to use PyMem_Free with yyjson's allocator. **/
void py_free(void* ctx, void* ptr);

extern yyjson_alc PyMem_Allocator;

#endif
