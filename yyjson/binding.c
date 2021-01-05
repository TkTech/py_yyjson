#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "yyjson.h"

PyMODINIT_FUNC
PyInit_cyyjson(void);

static inline PyObject *
element_to_primitive(yyjson_val *val);

/** wrapper to use PyMem_Malloc with yyjson's allocator. **/
static void *
py_malloc(void *Py_UNUSED(ctx), size_t size)
{
    return PyMem_Malloc(size);
}

/** wrapper to use PyMem_Realloc with yyjson's allocator. **/
static void *
py_realloc(void *Py_UNUSED(ctx), void *ptr, size_t size)
{
    return PyMem_Realloc(ptr, size);
}

/** wrapper to use PyMem_Free with yyjson's allocator. **/
static void
py_free(void *Py_UNUSED(ctx), void *ptr)
{
    PyMem_Free(ptr);
}

/**
 * A wrapper around PyMem_* to use is as a yyjson allocator.
 *
 * Use this as the argument to any method that takes an `alc` parameter.
 */
static yyjson_alc PyMem_Allocator = {py_malloc, py_realloc, py_free, NULL};

/**
 * Convert a Python object into a mutable yyjson value.
 *
 * :param doc: The document which the newly created value will be associated
 *             with. This does not mean that it is added to the document tree,
 *             only that its memory is managed together.
 * :param obj: The Python object to be converted.
 * :returns: A newly created yyjson_mut_val pointer.
 **/
static yyjson_mut_val *
mut_val_from_obj(yyjson_mut_doc *doc, PyObject *obj)
{
    // Lots of room for improvement here.
    if (obj == Py_True) {
        return yyjson_mut_true(doc);
    }
    else if (obj == Py_False) {
        return yyjson_mut_false(doc);
    }
    else if (obj == Py_None) {
        return yyjson_mut_null(doc);
    }
    else if (PyLong_CheckExact(obj)) {
        // We need an "unquoted string" type, we could just call
        // PyLong_Type.tp_str(obj) and return that, instead we have to do
        // overflow checks.
        //
        // Note this doesn't deal with CPython's compiled for 128bit
        // long longs. Avoid using ob_size and ob_digits, which are not
        // populated on pypy.
        int overflow;
        long long u_val = PyLong_AsLongLongAndOverflow(obj, &overflow);
        if (u_val == -1) {
            if (PyErr_Occurred() != NULL) {
                return NULL;
            }
            else if (overflow == -1) {
                PyErr_SetString(PyExc_OverflowError,
                                "Tried to serialize a number which exceeds the"
                                " limits.");
                return NULL;
            }
            else if (overflow == 1) {
                unsigned long long s_val = PyLong_AsUnsignedLongLong(obj);
                if (s_val == (unsigned long long)-1 &&
                    PyErr_Occurred() != NULL) {
                    return NULL;
                }
                return yyjson_mut_uint(doc, s_val);
            }
        }
        return yyjson_mut_int(doc, u_val);
    }
    else if (PyFloat_CheckExact(obj)) {
        return yyjson_mut_real(doc, PyFloat_AS_DOUBLE(obj));
    }
    else if (PyUnicode_Check(obj)) {
        Py_ssize_t size;
        const char *payload = PyUnicode_AsUTF8AndSize(obj, &size);
        if (payload == NULL) {
            return NULL;
        }
        return yyjson_mut_strncpy(doc, payload, size);
    }
    else if (PyDict_Check(obj)) {
        PyObject *key, *value;
        Py_ssize_t pos = 0;
        yyjson_mut_val *yy_dict = yyjson_mut_obj(doc);
        yyjson_mut_val *yy_key, *yy_value;

        while (PyDict_Next(obj, &pos, &key, &value)) {
            yy_key = mut_val_from_obj(doc, key);
            if (!yy_key)
                return NULL;
            yy_value = mut_val_from_obj(doc, value);
            if (!yy_value)
                return NULL;
            yyjson_mut_obj_add(yy_dict, yy_key, yy_value);
        }

        return yy_dict;
    }
    else if (PyList_Check(obj)) {
        Py_ssize_t length = PyList_GET_SIZE(obj);
        yyjson_mut_val *yy_list = yyjson_mut_arr(doc);
        yyjson_mut_val *yy_val;

        for (Py_ssize_t i = 0; i < length; i++) {
            yy_val = mut_val_from_obj(doc, PyList_GET_ITEM(obj, i));
            if (!yy_val)
                return NULL;
            yyjson_mut_arr_append(yy_list, yy_val);
        }

        return yy_list;
    }
    else {
        // Here's where we'd hook in a default serializer.
        PyErr_SetString(PyExc_ValueError,
                        "Tried to serialize an unknown type.");
        return NULL;
    }
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
            }
            else {
                Py_RETURN_FALSE;
            }
        case YYJSON_TYPE_NUM: {
            switch (yyjson_get_subtype(val)) {
                case YYJSON_SUBTYPE_UINT:
                    return PyLong_FromUnsignedLongLong(yyjson_get_uint(val));
                case YYJSON_SUBTYPE_SINT:
                    return PyLong_FromLongLong(yyjson_get_sint(val));
                case YYJSON_SUBTYPE_REAL:
                    return PyFloat_FromDouble(yyjson_get_real(val));
            }
        }
        case YYJSON_TYPE_STR: {
            size_t str_len = yyjson_get_len(val);
            const char *str = yyjson_get_str(val);

            return PyUnicode_FromStringAndSize(str, str_len);
        }
        case YYJSON_TYPE_ARR: {
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
        case YYJSON_TYPE_OBJ: {
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

                if (PyDict_SetItem(dict, py_key, py_val) == -1) {
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
 * Recursively convert the given value into an equivelent high-level Python
 * object.
 *
 * :param val: A pointer to the value to be converted.
 * :return: A pointer to a new PyObject representing the value.
 **/
static inline PyObject *
mut_element_to_primitive(yyjson_mut_val *val)
{
    yyjson_type type = yyjson_mut_get_type(val);

    switch (type) {
        case YYJSON_TYPE_NULL:
            Py_RETURN_NONE;
        case YYJSON_TYPE_BOOL:
            if (yyjson_mut_get_subtype(val) == YYJSON_SUBTYPE_TRUE) {
                Py_RETURN_TRUE;
            }
            else {
                Py_RETURN_FALSE;
            }
        case YYJSON_TYPE_NUM: {
            switch (yyjson_mut_get_subtype(val)) {
                case YYJSON_SUBTYPE_UINT:
                    return PyLong_FromUnsignedLongLong(
                        yyjson_mut_get_uint(val));
                case YYJSON_SUBTYPE_SINT:
                    return PyLong_FromLongLong(yyjson_mut_get_sint(val));
                case YYJSON_SUBTYPE_REAL:
                    return PyFloat_FromDouble(yyjson_mut_get_real(val));
            }
        }
        case YYJSON_TYPE_STR: {
            size_t str_len = yyjson_mut_get_len(val);
            const char *str = yyjson_mut_get_str(val);

            return PyUnicode_FromStringAndSize(str, str_len);
        }
        case YYJSON_TYPE_ARR: {
            PyObject *arr = PyList_New(yyjson_mut_arr_size(val));
            if (!arr) {
                return NULL;
            }

            yyjson_mut_val *obj_val;
            PyObject *py_val;

            yyjson_mut_arr_iter iter;
            yyjson_mut_arr_iter_init(val, &iter);

            size_t idx = 0;
            while ((obj_val = yyjson_mut_arr_iter_next(&iter))) {
                py_val = mut_element_to_primitive(obj_val);
                if (!py_val) {
                    return NULL;
                }

                PyList_SET_ITEM(arr, idx++, py_val);
            }

            return arr;
        }
        case YYJSON_TYPE_OBJ: {
            PyObject *dict = PyDict_New();
            if (!dict) {
                return NULL;
            }

            yyjson_mut_val *obj_key, *obj_val;
            PyObject *py_key, *py_val;

            yyjson_mut_obj_iter iter;
            yyjson_mut_obj_iter_init(val, &iter);

            while ((obj_key = yyjson_mut_obj_iter_next(&iter))) {
                obj_val = yyjson_mut_obj_iter_get_val(obj_key);

                py_key = mut_element_to_primitive(obj_key);
                py_val = mut_element_to_primitive(obj_val);

                if (!py_key) {
                    return NULL;
                }

                if (!py_val) {
                    Py_DECREF(py_key);
                    return NULL;
                }

                if (PyDict_SetItem(dict, py_key, py_val) == -1) {
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
PyDocument_dealloc(DocumentObject *self)
{
    if (self->i_doc != NULL)
        yyjson_doc_free(self->i_doc);
    if (self->m_doc != NULL)
        yyjson_mut_doc_free(self->m_doc);

    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
PyDocument_new(PyTypeObject *type, PyObject *Py_UNUSED(args),
             PyObject *Py_UNUSED(kwds))
{
    DocumentObject *self;
    self = (DocumentObject *)type->tp_alloc(type, 0);

    if (self != NULL) {
        self->i_doc = NULL;
        self->m_doc = NULL;
        self->is_mutable = false;
        self->alc = &PyMem_Allocator;
    }

    return (PyObject *)self;
}

static int
PyDocument_init(DocumentObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"content", NULL};

    char *content = NULL;
    Py_ssize_t content_len;
    yyjson_read_err err;
    PyObject *source = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O", kwlist, &source)) {
        return -1;
    }

    if (source && PyBytes_Check(source)) {
        if (PyBytes_AsStringAndSize(source, &content, &content_len) == -1) {
            return -1;
        }
    }
    else if (source && PyUnicode_Check(source)) {
        content = (char *)PyUnicode_AsUTF8AndSize(source, &content_len);
        if (!content) {
            return -1;
        }
    }

    if (!content && source) {
        self->m_doc = yyjson_mut_doc_new(self->alc);
        if (!self->m_doc) {
            PyErr_NoMemory();
            return -1;
        }
        self->is_mutable = true;

        yyjson_mut_val *val = mut_val_from_obj(self->m_doc, source);
        if (!val) {
            return -1;
        }

        yyjson_mut_doc_set_root(self->m_doc, val);
    }
    else if (content) {
        /* We're loading an existing document. */
        self->i_doc =
            yyjson_read_opts(content, content_len, 0, self->alc, &err);

        if (!self->i_doc) {
            // TODO: Error conversion!
            PyErr_SetString(PyExc_ValueError, err.msg);
            return -1;
        }
    }
    else {
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
PyDocument_as_obj(DocumentObject *self, void *Py_UNUSED(closure))
{
    if (self->m_doc) {
        yyjson_mut_val *root = yyjson_mut_doc_get_root(self->m_doc);
        return mut_element_to_primitive(root);
    }
    else if (self->i_doc) {
        yyjson_val *root = yyjson_doc_get_root(self->i_doc);
        return element_to_primitive(root);
    }
    else {
        PyErr_SetString(PyExc_ValueError, "Document not initialized!");
        return NULL;
    }
}

static PyGetSetDef PyDocument_members[] = {
    {"as_obj", (getter)PyDocument_as_obj, NULL,
     "The document as a native Python object.", NULL},
    {NULL} /* Sentinel */
};

/**
 * Dump a Document to a string.
 **/
static PyObject *
PyDocument_dumps(DocumentObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"pretty_print", "escape_unicode",
                             "escape_slashes", "allow_infinity", NULL};
    bool f_pretty_print = false;
    bool f_escape_unicode = false;
    bool f_escape_slashes = false;
    bool f_allow_infinity = false;
    yyjson_write_flag w_flag = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|$pppp", kwlist,
                                     &f_pretty_print, &f_escape_unicode,
                                     &f_escape_slashes, &f_allow_infinity)) {
        return NULL;
    }

    if (f_pretty_print)
        w_flag |= YYJSON_WRITE_PRETTY;
    if (f_escape_unicode)
        w_flag |= YYJSON_WRITE_ESCAPE_UNICODE;
    if (f_escape_slashes)
        w_flag |= YYJSON_WRITE_ESCAPE_SLASHES;
    if (f_allow_infinity)
        w_flag |= YYJSON_WRITE_ALLOW_INF_AND_NAN;

    char *result = NULL;
    size_t w_len;
    yyjson_write_err w_err;
    PyObject *obj_result = NULL;

    if (self->m_doc) {
        result = yyjson_mut_write_opts(self->m_doc, w_flag, self->alc, &w_len,
                                       &w_err);
    }
    else if (self->i_doc) {
        result =
            yyjson_write_opts(self->i_doc, w_flag, self->alc, &w_len, &w_err);
    }
    else {
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
 * Dump a Document to a string.
 **/
static PyObject *
PyDocument_get_pointer(DocumentObject *self, PyObject *args)
{
    char *pointer = NULL;
    Py_ssize_t pointer_len;

    if (!PyArg_ParseTuple(args, "s#", &pointer, &pointer_len)) {
        return NULL;
    }

    if (self->m_doc) {
        yyjson_mut_val *result =
            yyjson_mut_doc_get_pointer(self->m_doc, pointer);

        if (!result) {
            PyErr_SetString(PyExc_ValueError, "Not a valid JSON Pointer");
            return NULL;
        }

        return mut_element_to_primitive(result);
    }
    else if (self->i_doc) {
        yyjson_val *result = yyjson_doc_get_pointer(self->i_doc, pointer);

        if (!result) {
            PyErr_SetString(PyExc_ValueError, "Not a valid JSON Pointer");
            return NULL;
        }

        return element_to_primitive(result);
    }
    else {
        PyErr_SetString(PyExc_ValueError, "Document not initialized!");
        return NULL;
    }
}


/**
 * An implementation of JSON patch.
 *
 * Implements the `add`, `remove`, `replace`, `copy`, `move` and `test`
 * operations.
 *
 * When replacing elements, it's important for memory usage concerns to know
 * that yyjson cannot currently free memory. Any element you replace will
 * continue to exist until the document is destroyed.
 */
static PyObject *
PyDocument_patch(DocumentObject *self, PyObject *args)
{
    PyObject *obj = NULL;

    if (!PyArg_ParseTuple(args, "O", &obj)) {
        return NULL;
    }

    if (self->i_doc) {
        // If we've already loaded an immutable document, copy it to a mutable
        // variant and erase the old one.
        self->m_doc = yyjson_doc_mut_copy(self->i_doc, self->alc);
        self->is_mutable = true;
        yyjson_doc_free(self->i_doc);
        self->i_doc = NULL;
    } else if (!self->m_doc) {
        // Otherwise, create a new mutable document.
        self->m_doc = yyjson_mut_doc_new(self->alc);
    }

    if (!self->m_doc) {
        PyErr_NoMemory();
        return NULL;
    }

    yyjson_mut_val *val = mut_val_from_obj(self->m_doc, obj);
    if (!val) {
        return NULL;
    }

    if (yyjson_mut_doc_patch(self->m_doc, val)) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

static PyMethodDef PyDocument_methods[] = {
    {"dumps", (PyCFunction)(void (*)(void))PyDocument_dumps,
     METH_VARARGS | METH_KEYWORDS, "Dump the document to a string."},
    {"get_pointer", (PyCFunction)(void (*)(void))PyDocument_get_pointer,
     METH_VARARGS, "Get the element at the matching JSON Pointer (RFC 6901)."},
    {"patch", (PyCFunction)(void (*)(void))PyDocument_patch,
     METH_VARARGS, "Apply a single JSON Patch (RFC 6902) step."},
    {NULL} /* Sentinel */
};

static PyTypeObject DocumentType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "cyyjson.Document",
    .tp_doc = "A JSON Document.",
    .tp_basicsize = sizeof(DocumentObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = PyDocument_new,
    .tp_init = (initproc)PyDocument_init,
    .tp_dealloc = (destructor)PyDocument_dealloc,
    .tp_getset = PyDocument_members,
    .tp_methods = PyDocument_methods};

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
        Py_XDECREF(&DocumentType);
        Py_XDECREF(m);
        return NULL;
    }

    return m;
}
