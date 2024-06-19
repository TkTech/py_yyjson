#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "document.h"
#include "memory.h"
#include "decimal.h"
#include "yyjson.h"

PyObject *YY_DecimalModule = NULL;
PyObject *YY_DecimalClass = NULL;

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

  // We need to pre-import the Decimal module to have it available globally.
  YY_DecimalModule = PyImport_ImportModule("decimal");
  if (YY_DecimalModule == NULL) {
    return NULL;
  }
  Py_INCREF(YY_DecimalModule);

  YY_DecimalClass = PyObject_GetAttrString(YY_DecimalModule, "Decimal");
  if (YY_DecimalClass == NULL) {
    return NULL;
  }
  Py_INCREF(YY_DecimalClass);

  return m;
}
