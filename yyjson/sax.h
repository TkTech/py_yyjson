#ifndef PY_YYJSON_SAX_H
#define PY_YYJSON_SAX_H

#define PY_SSIZE_T_CLEAN
#include <Python.h>

/**
 * Module-level method table exporting ``sax(...)``. Assigned to the module's
 * ``m_methods`` in binding.c. See sax.c for the implementation and docstring.
 */
extern PyMethodDef yyjson_sax_methods[];

#endif
