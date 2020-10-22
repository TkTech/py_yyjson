#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "yyjson.h"

static inline PyObject * element_to_primitive(yyjson_val *val);


static inline PyObject *
element_to_primitive(yyjson_val *val)
{
    yyjson_type type = yyjson_get_type(val);

    switch (type) {
        case YYJSON_TYPE_NULL:
            Py_RETURN_NONE;
        case YYJSON_TYPE_BOOL:
            if (yyjson_get_subtype(val) == YYJSON_SUBTYPE_TRUE) {
                Py_RETURN_TRUE;
            } else {
                Py_RETURN_FALSE;
            }
        case YYJSON_TYPE_NUM:
        {
            switch (yyjson_get_subtype(val)) {
                case YYJSON_SUBTYPE_UINT:
                    return PyLong_FromUnsignedLongLong(yyjson_get_uint(val));
                case YYJSON_SUBTYPE_SINT:
                    return PyLong_FromLongLong(yyjson_get_sint(val));
                case YYJSON_SUBTYPE_REAL:
                    return PyFloat_FromDouble(yyjson_get_real(val));
            }
        }
        case YYJSON_TYPE_STR:
        {
            size_t str_len = yyjson_get_len(val);
            const char *str = yyjson_get_str(val);

            return PyUnicode_FromStringAndSize(str, str_len);
        }
        case YYJSON_TYPE_ARR:
        {
            PyObject *arr = PyList_New(yyjson_arr_size(val));
            if (!arr) {
                return NULL;
            }

            yyjson_val *obj_val;
            PyObject *py_val;

            yyjson_arr_iter iter;
            yyjson_arr_iter_init(val, &iter);

            size_t idx = 0;
            while ((obj_val = yyjson_arr_iter_next(&iter))) {
                py_val = element_to_primitive(obj_val);
                if (!py_val) {
                    return NULL;
                }

                PyList_SET_ITEM(arr, idx++, py_val);
            }

            return arr;
        }
        case YYJSON_TYPE_OBJ:
        {
            PyObject *dict = PyDict_New();
            if (!dict) {
                return NULL;
            }

            yyjson_val *obj_key, *obj_val;
            PyObject *py_key, *py_val;

            yyjson_obj_iter iter;
            yyjson_obj_iter_init(val, &iter);

            while ((obj_key = yyjson_obj_iter_next(&iter))) {
                obj_val = yyjson_obj_iter_get_val(obj_key);

                py_key = element_to_primitive(obj_key);
                py_val = element_to_primitive(obj_val);

                if (!py_key) {
                    return NULL;
                }

                if (!py_val) {
                    Py_DECREF(py_key);
                    return NULL;
                }

                if(PyDict_SetItem(dict, py_key, py_val) == -1) {
                    return NULL;
                }

                Py_DECREF(py_key);
                Py_DECREF(py_val);
            }
            return dict;
        }
        case YYJSON_TYPE_NONE:
        default:
            // TODO: PANIC
            break;
    }
}


typedef struct {
    PyObject_HEAD
    yyjson_doc *doc;
} DocumentObject;

static void
Document_dealloc(DocumentObject *self)
{
    if (self->doc != NULL) {
        yyjson_doc_free(self->doc);
    }

    Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyObject *
Document_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    DocumentObject *self;
    self = (DocumentObject *) type->tp_alloc(type, 0);

    if (self != NULL) {
        char *content = NULL;
        static char *kwlist = {"content"};
        Py_ssize_t content_len;
        yyjson_read_err err;

        if(!PyArg_ParseTupleAndKeywords(args, kwds, "|s#", &kwlist, &content,
                                        &content_len)) {
            return NULL;
        }

        self->doc = yyjson_read_opts(
            content,
            content_len,
            0,
            NULL,
            &err
        );

        if (!self->doc) {
            // TODO: Error conversion!
            PyErr_SetString(PyExc_ValueError, err.msg);
            return NULL;
        }
    }

    return (PyObject *) self;
}

static int
Document_init(DocumentObject *self, PyObject *args, PyObject *kwds)
{
    return 0;
}

static PyObject *
Document_get_size(DocumentObject *self, void* closure)
{
    return PyLong_FromLong(yyjson_doc_get_read_size(self->doc));
}

static PyObject *
Document_get_count(DocumentObject *self, void* closure)
{
    return PyLong_FromLong(yyjson_doc_get_val_count(self->doc));
}

static PyObject *
Document_as_obj(DocumentObject *self, void* closure)
{
    yyjson_val *root = yyjson_doc_get_root(self->doc);
    return element_to_primitive(root);
}

static PyGetSetDef Document_members[] = {
    {"size",
        (getter)Document_get_size,
        NULL,
        "Size read in bytes.",
        NULL
    },
    {"count",
        (getter)Document_get_count,
        NULL,
        "The total number of elements in this document.",
        NULL
    },
    {"as_obj",
        (getter)Document_as_obj,
        NULL,
        "The parsed document as a native Python object.",
        NULL
    },
    {NULL} /* Sentinel */
};

static PyMethodDef Document_methods[] = {
    NULL /* Sentinel */
};

static PyTypeObject DocumentType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cyyjson.Document",
    .tp_doc = "yyjson Document",
    .tp_basicsize = sizeof(DocumentObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Document_new,
    .tp_init = (initproc) Document_init,
    .tp_dealloc = (destructor) Document_dealloc,
    .tp_getset = Document_members,
    .tp_methods = Document_methods
};

static PyModuleDef yymodule = {
    PyModuleDef_HEAD_INIT,
    .m_name = "cyyjson",
    .m_doc = "yyjson Python Bindings.",
    .m_size = -1
};

PyMODINIT_FUNC
PyInit_cyyjson(void)
{
    PyObject *m;
    if (PyType_Ready(&DocumentType) < 0) {
        return NULL;
    }

    m = PyModule_Create(&yymodule);
    if (m == NULL) {
        return NULL;
    }

    Py_INCREF(&DocumentType);
    if (PyModule_AddObject(m, "Document", (PyObject *)&DocumentType) < 0) {
        Py_DECREF(&DocumentType);
        Py_DECREF(m);
        return NULL;
    }

    return m;
}
