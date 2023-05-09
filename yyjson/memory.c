#include "memory.h"

/** wrapper to use PyMem_Malloc with yyjson's allocator. **/
void* py_malloc(void* ctx, size_t size) { return PyMem_Malloc(size); }

/** wrapper to use PyMem_Realloc with yyjson's allocator. **/
void* py_realloc(void* ctx, void* ptr, size_t old_size, size_t size) {
  return PyMem_Realloc(ptr, size);
}

/** wrapper to use PyMem_Free with yyjson's allocator. **/
void py_free(void* ctx, void* ptr) { PyMem_Free(ptr); }

yyjson_alc PyMem_Allocator = {py_malloc, py_realloc, py_free, NULL};
