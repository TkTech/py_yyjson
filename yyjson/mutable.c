#include "memory.h"
#include "mutable.h"

static inline PyObject * mut_element_to_primitive(yyjson_mut_val *val);

/**
 * Recursively convert the given value into an equivalent high-level Python
 * object.
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
            } else {
                Py_RETURN_FALSE;
            }
        case YYJSON_TYPE_NUM:
        {
            switch (yyjson_mut_get_subtype(val)) {
                case YYJSON_SUBTYPE_UINT:
                    return PyLong_FromUnsignedLongLong(yyjson_mut_get_uint(val));
                case YYJSON_SUBTYPE_SINT:
                    return PyLong_FromLongLong(yyjson_mut_get_sint(val));
                case YYJSON_SUBTYPE_REAL:
                    return PyFloat_FromDouble(yyjson_mut_get_real(val));
            }
        }
        case YYJSON_TYPE_STR:
        {
            size_t str_len = yyjson_mut_get_len(val);
            const char *str = yyjson_mut_get_str(val);

            return PyUnicode_FromStringAndSize(str, str_len);
        }
        case YYJSON_TYPE_ARR:
        {
            PyObject *arr = PyList_New(yyjson_mut_arr_size(val));
            if (!arr) {
                return NULL;
            }

            yyjson_mut_val *obj_val;
            PyObject *py_val;

            yyjson_mut_arr_iter iter = {0};
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
        case YYJSON_TYPE_OBJ:
        {
            PyObject *dict = PyDict_New();
            if (!dict) {
                return NULL;
            }

            yyjson_mut_val *obj_key, *obj_val;
            PyObject *py_key, *py_val;

            yyjson_mut_obj_iter iter = {0};
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

static void
MutableDocument_dealloc(MutableDocumentObject *self)
{
    if (self->m_doc != NULL) yyjson_mut_doc_free(self->m_doc);
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyObject *
MutableDocument_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    MutableDocumentObject *self;
    self = (MutableDocumentObject *) type->tp_alloc(type, 0);

    if (self != NULL) {
        self->m_doc = NULL;
        self->alc = &PyMem_Allocator;
    }

    return (PyObject *) self;
}

PyDoc_STRVAR(
    MutableDocument_init_doc,
    "A single mutable JSON document.\n"
);
static int
MutableDocument_init(MutableDocumentObject *self, PyObject *args, PyObject *kwds)
{
    char *content = NULL;
    static char *kwlist[] = {"content", "flags", NULL};
    Py_ssize_t content_len;
    yyjson_read_err err;
    yyjson_read_flag r_flag = 0;
    yyjson_doc *i_doc = NULL;

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

    i_doc = yyjson_read_opts(
        content,
        content_len,
        r_flag,
        self->alc,
        &err
    );

    if (!i_doc) {
        // TODO: Error conversion!
        PyErr_SetString(PyExc_ValueError, err.msg);
        return -1;
    }

    self->m_doc = yyjson_doc_mut_copy(i_doc, self->alc);

    return 0;
}

/**
 * Recursively convert the document into Python objects.
 */
static PyObject *
MutableDocument_as_obj(MutableDocumentObject *self, void* closure)
{
    yyjson_mut_val *root = yyjson_mut_doc_get_root(self->m_doc);
    return mut_element_to_primitive(root);
}

static PyMethodDef MutableDocument_methods[] = {
    {NULL} /* Sentinel */
};

static PyGetSetDef MutableDocument_members[] = {
    {"as_obj",
        (getter)MutableDocument_as_obj,
        NULL,
        "The document as a native Python object.",
        NULL
    },
    {NULL} /* Sentinel */
};

PyTypeObject MutableDocumentType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cyyjson.MutableDocument",
    .tp_doc = MutableDocument_init_doc,
    .tp_basicsize = sizeof(MutableDocumentObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = MutableDocument_new,
    .tp_init = (initproc) MutableDocument_init,
    .tp_dealloc = (destructor) MutableDocument_dealloc,
    .tp_getset = MutableDocument_members,
    .tp_methods = MutableDocument_methods
};
