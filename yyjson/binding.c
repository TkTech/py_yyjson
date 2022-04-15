#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "yyjson.h"

static inline PyObject * element_to_primitive(yyjson_val *val);

/** wrapper to use PyMem_Malloc with yyjson's allocator. **/
static void*
py_malloc(void *ctx, size_t size)
{
    return PyMem_Malloc(size);
}

/** wrapper to use PyMem_Realloc with yyjson's allocator. **/
static void*
py_realloc(void *ctx, void *ptr, size_t size)
{
    return PyMem_Realloc(ptr, size);
}

/** wrapper to use PyMem_Free with yyjson's allocator. **/
static void
py_free(void *ctx, void *ptr)
{
    PyMem_Free(ptr);
}

static yyjson_alc PyMem_Allocator = {
    py_malloc,
    py_realloc,
    py_free,
    NULL
};

/**
 * Recursively convert the given value into an equivalent high-level Python
 * object.
 *
 * :param val: A pointer to the value to be converted.
 * :return: A pointer to a new PyObject representing the value.
 **/
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

            yyjson_arr_iter iter = {0};
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

            yyjson_obj_iter iter = {0};
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
            PyErr_SetString(PyExc_TypeError, "Unknown tape type encountered.");
            return NULL;
    }
}


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

static void
Document_dealloc(DocumentObject *self)
{
    if (self->i_doc != NULL) yyjson_doc_free(self->i_doc);
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyObject *
Document_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    DocumentObject *self;
    self = (DocumentObject *) type->tp_alloc(type, 0);

    if (self != NULL) {
        self->i_doc = NULL;
        self->alc = &PyMem_Allocator;
    }

    return (PyObject *) self;
}

PyDoc_STRVAR(
    Document_init_doc,
    "A single JSON document.\n"
);
static int
Document_init(DocumentObject *self, PyObject *args, PyObject *kwds)
{
    char *content = NULL;
    static char *kwlist[] = {"content", "flags", NULL};
    Py_ssize_t content_len;
    yyjson_read_err err;
    yyjson_read_flag r_flag = 0;

    if(!PyArg_ParseTupleAndKeywords(
            args,
            kwds,
            "s#|$I",
            kwlist,
            &content,
            &content_len,
            &r_flag
        )) {
        return -1;
    }

    self->i_doc = yyjson_read_opts(
        content,
        content_len,
        r_flag,
        self->alc,
        &err
    );

    if (!self->i_doc) {
        // TODO: Error conversion!
        PyErr_SetString(PyExc_ValueError, err.msg);
        return -1;
    }

    return 0;
}

/**
 * Recursively convert the document into Python objects.
 */
static PyObject *
Document_as_obj(DocumentObject *self, void* closure)
{
    yyjson_val *root = yyjson_doc_get_root(self->i_doc);
    return element_to_primitive(root);
}

static PyGetSetDef Document_members[] = {
    {"as_obj",
        (getter)Document_as_obj,
        NULL,
        "The document as a native Python object.",
        NULL
    },
    {NULL} /* Sentinel */
};

PyDoc_STRVAR(
    Document_dumps_doc,
    "Dumps the document to a string and returns it.\n"
    "\n"
    ":param flags: Flags that control JSON writing behaviour.\n"
    ":type flags: :class:`yyjson.WriterFlags`, optional"
);
static PyObject *
Document_dumps(DocumentObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {
        "flags",
        NULL
    };
    yyjson_write_flag w_flag = 0;

    if(!PyArg_ParseTupleAndKeywords(
            args,
            kwds,
            "|$I",
            kwlist,
            &w_flag
        )) {
        return NULL;
    }

    char *result = NULL;
    size_t w_len;
    yyjson_write_err w_err;
    PyObject *obj_result = NULL;

    result = yyjson_write_opts(
        self->i_doc,
        w_flag,
        self->alc,
        &w_len,
        &w_err
    );

    if (!result) {
        PyErr_SetString(PyExc_ValueError, w_err.msg);
        return NULL;
    }

    obj_result = PyUnicode_FromStringAndSize(result, w_len);
    self->alc->free(NULL, result);

    return obj_result;
}

PyDoc_STRVAR(
    Document_get_pointer_doc,
    "Returns the JSON element at the given JSON pointer (RFC 6901).\n"
    "\n"
    ":param pointer: JSON Pointer to search for.\n"
    ":type pointer: str"
);
static PyObject *
Document_get_pointer(DocumentObject *self, PyObject *args)
{
    char *pointer = NULL;
    Py_ssize_t pointer_len;

    if (!PyArg_ParseTuple(
            args,
            "s#",
            &pointer,
            &pointer_len
        )) {
        return NULL;
    }

    yyjson_val *result = yyjson_doc_get_pointer(
        self->i_doc,
        pointer
    );

    if (!result) {
        PyErr_SetString(PyExc_ValueError, "Not a valid JSON Pointer");
        return NULL;
    }

    return element_to_primitive(result);
}

static Py_ssize_t
Document_length(DocumentObject *self)
{
    yyjson_val *root = yyjson_doc_get_root(self->i_doc);
    yyjson_type type = yyjson_get_type(root);
    if (type == YYJSON_TYPE_OBJ) {
        return yyjson_obj_size(root);
    } else if (type == YYJSON_TYPE_ARR) {
        return yyjson_arr_size(root);
    }

    PyErr_SetString(
        PyExc_TypeError,
        "Can't get the length of element that isn't an object or array."
    );
    return -1;
}

static PyMethodDef Document_methods[] = {
    {"dumps",
        (PyCFunction)(void(*)(void))Document_dumps,
        METH_VARARGS | METH_KEYWORDS,
        Document_dumps_doc
    },
    {"get_pointer",
        (PyCFunction)(void(*)(void))Document_get_pointer,
        METH_VARARGS,
        Document_get_pointer_doc
    },
    {NULL} /* Sentinel */
};

static PyMappingMethods Document_mapping_methods = {
    (lenfunc)Document_length,
    NULL,
    NULL
};


static PyTypeObject DocumentType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cyyjson.Document",
    .tp_doc = Document_init_doc,
    .tp_basicsize = sizeof(DocumentObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Document_new,
    .tp_init = (initproc) Document_init,
    .tp_dealloc = (destructor) Document_dealloc,
    .tp_getset = Document_members,
    .tp_as_mapping = &Document_mapping_methods,
    .tp_methods = Document_methods
};

static PyModuleDef yymodule = {
    PyModuleDef_HEAD_INIT,
    .m_name = "cyyjson",
    .m_doc = "Python bindings for the yyjson project.",
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
