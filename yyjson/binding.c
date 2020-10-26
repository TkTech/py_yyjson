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

/**
 * A wrapper around PyMem_* to use is as a yyjson allocator.
 *
 * Use this as the argument to any method that takes an `alc` parameter.
 */
static yyjson_alc PyMem_Allocator = {
    py_malloc,
    py_realloc,
    py_free,
    NULL
};

/**
 * Find the yyjson_mut_val at the given pointer.
 **/
static yyjson_mut_val*
mut_val_at_pointer(yyjson_mut_val *start, const char *ptr)
{
    // Short circuit for requesting the "current object".
    if (strncmp(ptr, ".", 2) == 0) return start;
    return NULL;
}

/**
 * Convert a Python object into a mutable yyjson value.
 **/
static yyjson_mut_val*
mut_val_from_obj(yyjson_mut_doc *doc, PyObject *obj)
{
    if (PyLong_CheckExact(obj)) {

    } else if (PyFloat_CheckExact(obj)) {

    } else if (obj == Py_True) {
        return yyjson_mut_true(doc);
    } else if (obj == Py_False) {
        return yyjson_mut_false(doc);
    } else if (PyUnicode_Check(obj)) {

    } else if (obj == Py_None) {
        return yyjson_mut_null(doc);
    } else {
        // !_!
        return NULL;
    }

    return NULL;
}

/**
 * Recursively convert the given value into an equivelent high-level Python
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
            PyErr_SetString(
                PyExc_TypeError,
                "Unknown tape type encountered."
            );
            return NULL;
    }
}


/**
 * Represents a yyjson document.
 *
 * When an immutable document is modified, it is transparently copied into
 * a mutable document and the immutable copy is destroyed.
 */
typedef struct {
    PyObject_HEAD
    /** An immutable parsed document. */
    yyjson_doc *i_doc;
    /** A mutable document, either copied from i_doc on modification, or
     * created from scratch.
     **/
    yyjson_mut_doc *m_doc;
    /** Is this document mutable? */
    bool is_mutable;
    /** The memory allocator in use for this document. */
    yyjson_alc *alc;
} DocumentObject;

static void
Document_dealloc(DocumentObject *self)
{
    if (self->i_doc != NULL) yyjson_doc_free(self->i_doc);
    if (self->m_doc != NULL) yyjson_mut_doc_free(self->m_doc);

    Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyObject *
Document_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    DocumentObject *self;
    self = (DocumentObject *) type->tp_alloc(type, 0);

    if (self != NULL) {
        self->i_doc = NULL;
        self->m_doc = NULL;
        self->is_mutable = false;
        self->alc = &PyMem_Allocator;
    }

    return (PyObject *) self;
}

static int
Document_init(DocumentObject *self, PyObject *args, PyObject *kwds)
{
    char *content = NULL;
    static char *kwlist[] = {"content", NULL};
    Py_ssize_t content_len;
    yyjson_read_err err;

    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|s#", kwlist, &content,
                                    &content_len)) {
        return -1;
    }

    if (content) {
        /* We're loading an existing document. */
        self->i_doc = yyjson_read_opts(
            content,
            content_len,
            0,
            self->alc,
            &err
        );

        if (!self->i_doc) {
            // TODO: Error conversion!
            PyErr_SetString(PyExc_ValueError, err.msg);
            return -1;
        }
    } else {
        /* We're creating a new document from scratch. */
        self->m_doc = yyjson_mut_doc_new(self->alc);
        if (!self->m_doc) {
            PyErr_NoMemory();
            return -1;
        }
        self->is_mutable = true;
    }

    return 0;
}

/**
 * Recursively convert the document into Python objects.
 */
static PyObject *
Document_as_obj(DocumentObject *self, void* closure)
{
    if (self->i_doc) {
        yyjson_val *root = yyjson_doc_get_root(self->i_doc);
        return element_to_primitive(root);
    } else {
        PyErr_SetString(PyExc_ValueError, "Document not initialized!");
        return NULL;
    }
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

/**
 * Dump a Document to a string.
 **/
static PyObject *
Document_dumps(DocumentObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {
        "pretty_print",
        "escape_unicode",
        "escape_slashes",
        "allow_infinity",
        NULL
    };
    bool f_pretty_print = false;
    bool f_escape_unicode = false;
    bool f_escape_slashes = false;
    bool f_allow_infinity = false;
    yyjson_write_flag w_flag = 0;

    if(!PyArg_ParseTupleAndKeywords(
            args,
            kwds,
            "|$pppp",
            kwlist,
            &f_pretty_print,
            &f_escape_unicode,
            &f_escape_slashes,
            &f_allow_infinity
        )) {
        return NULL;
    }

    if (f_pretty_print) w_flag |= YYJSON_WRITE_PRETTY;
    if (f_escape_unicode) w_flag |= YYJSON_WRITE_ESCAPE_UNICODE;
    if (f_escape_slashes) w_flag |= YYJSON_WRITE_ESCAPE_SLASHES;
    if (f_allow_infinity) w_flag |= YYJSON_WRITE_ALLOW_INF_AND_NAN;

    char *result = NULL;
    size_t w_len;
    yyjson_write_err w_err;
    PyObject *obj_result = NULL;

    if (self->m_doc) {
        result = yyjson_mut_write_opts(
            self->m_doc,
            w_flag,
            self->alc,
            &w_len,
            &w_err
        );
    } else if (self->i_doc) {
        result = yyjson_write_opts(
            self->i_doc,
            w_flag,
            self->alc,
            &w_len,
            &w_err
        );
    } else {
        PyErr_SetString(PyExc_ValueError, "Document not initialized!");
        return NULL;
    }

    if (!result) {
        PyErr_SetString(PyExc_ValueError, w_err.msg);
        return NULL;
    }

    obj_result = PyUnicode_FromStringAndSize(result, w_len);
    self->alc->free(NULL, result);

    return obj_result;
}

/**
 * Patches a document with the passed object at the point specified by
 * path, or the root by defualt.
 */
static PyObject *
Document_patch(DocumentObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {
        "obj",
        "path",
        NULL
    };
    char *path = NULL;
    Py_ssize_t path_len = 0;
    PyObject *obj = NULL;

    if(!PyArg_ParseTupleAndKeywords(
            args,
            kwds,
            "O|s#",
            kwlist,
            &obj,
            &path,
            &path_len
        )) {
        return NULL;
    }

    if (!self->is_mutable) {
        self->m_doc = yyjson_doc_mut_copy(
            self->i_doc,
            self->alc
        );
        if (!self->m_doc) {
            PyErr_SetString(
                PyExc_RuntimeError,
                "Unable to convert immutable document to mutable."
            );
            return NULL;
        }
        self->is_mutable = true;
    }

    // A document must already have a root, unless we're patching the root,
    // which ends up just creating it.
    yyjson_mut_val *target = yyjson_mut_doc_get_root(self->m_doc);
    if (!target && (path != NULL && strncmp(path, ".", 2) != 0)) {
        PyErr_SetString(
            PyExc_ValueError,
            "Tried to patch a document with no root element."
        );
        return NULL;
    }

    // Find the element we're supposed to be patching.
    if (path != NULL) {
        // This will set a Python error if something occured, so just
        // return for the raise.
        target = mut_val_at_pointer(target, path);
        if (!target) return NULL;
    }

    yyjson_mut_val *patch = mut_val_from_obj(self->m_doc, obj);

    // If we're here and still have no target, it's because we're building
    // the root. Since this is so common, shortcut it.
    if (!target) {
        yyjson_mut_doc_set_root(self->m_doc, patch);
        Py_RETURN_NONE;
    }

    return NULL;
}

static PyMethodDef Document_methods[] = {
    {"dumps",
        (PyCFunction)(void(*)(void))Document_dumps,
        METH_VARARGS | METH_KEYWORDS,
        "Dump the document to a string."
    },
    {"patch",
        (PyCFunction)(void(*)(void))Document_patch,
        METH_VARARGS | METH_KEYWORDS,
        "Patch the document at a given point."
    },
    {NULL} /* Sentinel */
};


static PyTypeObject DocumentType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cyyjson.Document",
    .tp_doc = "A JSON Document.",
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
