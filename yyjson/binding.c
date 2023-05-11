#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "document.h"
#include "memory.h"
#include "yyjson.h"

static PyModuleDef yymodule = {
    PyModuleDef_HEAD_INIT, .m_name = "cyyjson",
    .m_doc = "Python bindings for the yyjson project.", .m_size = -1};

PyMODINIT_FUNC PyInit_cyyjson(void) {
  PyObject* m;

  if (PyType_Ready(&DocumentType) < 0) {
    return NULL;
  }

  m = PyModule_Create(&yymodule);
  if (m == NULL) {
    return NULL;
  }

  Py_INCREF(&DocumentType);
  if (PyModule_AddObject(m, "Document", (PyObject*)&DocumentType) < 0) {
    Py_DECREF(&DocumentType);
    Py_DECREF(m);
    return NULL;
  }

  return m;
}
