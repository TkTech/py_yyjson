#include "memory.h"
#include "document.h"

#define ENSURE_MUTABLE(self) \
    if (self->i_doc) { \
        self->m_doc = yyjson_doc_mut_copy(self->i_doc, self->alc); \
        yyjson_doc_free(self->i_doc); \
    }

static inline PyObject * element_to_primitive(yyjson_val *val);
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
        case YYJSON_TYPE_RAW:
            return PyLong_FromString(yyjson_mut_get_raw(val), NULL, 10);
        case YYJSON_TYPE_NONE:
        default:
            PyErr_SetString(PyExc_TypeError, "Unknown tape type encountered.");
            return NULL;
    }
}

/**
 * Recursively convert the given value into an equivalent high-level Python
 * object.
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
        case YYJSON_TYPE_RAW:
            return PyLong_FromString(yyjson_get_raw(val), NULL, 10);
        case YYJSON_TYPE_NONE:
        default:
            PyErr_SetString(PyExc_TypeError, "Unknown tape type encountered.");
            return NULL;
    }
}


PyTypeObject *type_for_conversion(PyObject *obj) {
    if (obj->ob_type == &PyUnicode_Type) {
        return &PyUnicode_Type;
    } else if (obj->ob_type == &PyLong_Type) {
        return &PyLong_Type;
    } else if (obj->ob_type == &PyFloat_Type) {
        return &PyFloat_Type;
    } else if (obj->ob_type == &PyDict_Type) {
        return &PyDict_Type;
    } else if (obj->ob_type == &PyList_Type) {
        return &PyList_Type;
    } else if (obj->ob_type == &PyBool_Type) {
        return &PyBool_Type;
    } else if (obj->ob_type == Py_None->ob_type) {
        return Py_None->ob_type;
    }
    return NULL;
}


/**
 * Recursively convert a Python object into yyjson elements.
 */
static inline yyjson_mut_val *
mut_primitive_to_element(yyjson_mut_doc *doc, PyObject *obj) {
    const PyTypeObject *ob_type = type_for_conversion(obj);

    if (ob_type == &PyUnicode_Type) {
        Py_ssize_t str_len;
        const char *str = PyUnicode_AsUTF8AndSize(obj, &str_len);
        return yyjson_mut_strncpy(doc, str, str_len);
    } else if (ob_type == &PyLong_Type) {
        // Serialization of integers is a little special, since Python allows
        // integers of (effectively) any size. While > 53bit is technically
        // against the spec, at least 64bit is widely supported and the builtin
        // Python JSON module supports integers of any size.
        int overflow = 0;
        const int64_t num = PyLong_AsLongLongAndOverflow(obj, &overflow);
        if (!overflow) {
            if (num == -1 && PyErr_Occurred()) return NULL;
            return yyjson_mut_sint(doc, num);
        } else {
            // Number overflowed, try an unsigned long long.
            const uint64_t unum = PyLong_AsUnsignedLongLong(obj);
            if (unum == (uint64_t)-1 && PyErr_Occurred()) {
                // Number might have been too large even for a unit64_t, resort
                // to a raw type by converting the number to its string
                // representation.
                PyErr_Clear(); // Erase the OverflowError
                PyObject *str_repr = PyObject_Str(obj);
                Py_ssize_t str_len;
                const char *str = PyUnicode_AsUTF8AndSize(str_repr, &str_len);
                return yyjson_mut_rawncpy(doc, str, str_len);
            } else {
                return yyjson_mut_uint(doc, unum);
            }
        }
    } else if (ob_type == &PyList_Type) {
        yyjson_mut_val *val = yyjson_mut_arr(doc);
        for (Py_ssize_t i = 0; i < PyList_GET_SIZE(obj); i++) {
            yyjson_mut_arr_append(
                val,
                mut_primitive_to_element(doc, PyList_GET_ITEM(obj, i))
            );
        }
        return val;
    } else if (ob_type == &PyDict_Type) {
        yyjson_mut_val *val = yyjson_mut_obj(doc);
        Py_ssize_t i = 0;
        PyObject *key, *value;
        while (PyDict_Next(obj, &i, &key, &value)) {
            yyjson_mut_obj_add(
                val,
                // TODO: Keys of valid JSON documents will always be a string,
                // we should hot-path this.
                mut_primitive_to_element(doc, key),
                mut_primitive_to_element(doc, value)
            );
        }
        return val;
    } else if (ob_type == &PyFloat_Type) {
        double dnum = PyFloat_AsDouble(obj);
        if (dnum == -1 && PyErr_Occurred()) return NULL;
        return yyjson_mut_real(doc, dnum);
    } else if (obj == Py_True) {
        return yyjson_mut_true(doc);
    } else if (obj == Py_False) {
        return yyjson_mut_false(doc);
    } else if (obj == Py_None) {
        return yyjson_mut_null(doc);
    } else {
        PyErr_SetString(
            PyExc_TypeError,
            // TODO: We can provide a much better error here. Also add support
            // for a default hook.
            "Tried to serialize an object we don't know how to handle."
        );
        return NULL;
    }
}

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
        self->alc = &PyMem_Allocator;
    }

    return (PyObject *) self;
}

PyDoc_STRVAR(
    Document_init_doc,
    "A single JSON document.\n"
    "\n"
    "A `Document` can be built from a JSON-serializable Python object,\n"
    "a JSON document in a ``str``, or a JSON document encoded to ``bytes``.\n"
    "Ex:\n"
    "\n"
    ".. doctest::\n"
    "\n"
    "   >>> Document({'a': 1, 'b': 2})\n"
    "   >>> Document(b'{\"a\": 1, \"b\": 2}')\n"
    "   >>> Document('{\"a\": 1, \"b\": 2}')\n"
    "\n"
    "By default, the parsing is strict and follows the JSON specifications.\n"
    "You can change this behaviour by passing in :class:`ReaderFlags`. Ex:\n"
    "\n"
    ".. doctest::\n"
    "\n"
    "   >>> Document('''{\n"
    "   ...     // Comments in JSON!?!?\n"
    "   ...     \"a\": 1\n"
    "   ... }''', flags=ReaderFlags.ALLOW_COMMENTS)\n"
    "\n"
    ":param content: The initial content of the document.\n"
    ":type content: A JSON ``str`` or ``bytes``, or a Python object that can\n"
    "               be serialized.\n"
    ":param flags: Flags that modify the document parsing behaviour.\n"
    ":type flags: :class:`ReaderFlags`, optional"
);
static int
Document_init(DocumentObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"content", "flags", NULL};
    PyObject *content;
    yyjson_read_err err;
    yyjson_read_flag r_flag = 0;

    if(!PyArg_ParseTupleAndKeywords(
            args,
            kwds,
            "O|$I",
            kwlist,
            &content,
            &r_flag
        )) {
        return -1;
    }

    // We were given a string, so just parse it into an immutable document.
    if (PyUnicode_Check(content) || PyBytes_Check(content)) {
        Py_ssize_t content_len;
        const char *content_as_utf8 = NULL;

        if (PyUnicode_Check(content)) {
            content_as_utf8 = PyUnicode_AsUTF8AndSize(content, &content_len);
        } else {
            PyBytes_AsStringAndSize(content, (char **)&content_as_utf8, &content_len);
        }

        self->i_doc = yyjson_read_opts(
            // As long as we don't expose the insitu reader flag, it's safe to
            // discard the const here.
            (char *)content_as_utf8,
            content_len,
            r_flag,
            self->alc,
            &err
        );

        if (!self->i_doc) {
            // TODO: Error conversion! ValueError is not always the right
            // choice, although JSONDecodeError is a ValueError subclass.
            PyErr_SetString(PyExc_ValueError, err.msg);
            return -1;
        }
    } else {
        self->m_doc = yyjson_mut_doc_new(self->alc);
        yyjson_mut_val *val = mut_primitive_to_element(self->m_doc, content);
        if (val == NULL) {
            return -1;
        }
        yyjson_mut_doc_set_root(self->m_doc, val);
    }

    return 0;
}

/**
 * Recursively convert the document into Python objects.
 */
static PyObject *
Document_as_obj(DocumentObject *self, void* closure)
{
    if (self->i_doc != NULL) {
        yyjson_val *root = yyjson_doc_get_root(self->i_doc);
        return element_to_primitive(root);
    } else {
        yyjson_mut_val *root = yyjson_mut_doc_get_root(self->m_doc);
        return mut_element_to_primitive(root);
    }
}

PyDoc_STRVAR(
    Document_dumps_doc,
    "Dumps the document to a string and returns it.\n"
    "\n"
    "By default, serializes to a minified string and strictly follows the\n"
    "JSON specification. Ex:\n"
    "\n"
    ".. doctest::\n"
    "\n"
    "    >>> doc = Document({'hello': 'world'})\n"
    "    >>> print(doc.dumps())\n"
    "    {\"hello\":\"world\"}\n"
    "\n"
    "This behaviour can be controlled by passing :class:`WriterFlags`. Ex:\n"
    "\n"
    ".. doctest::\n"
    "\n"
    "    >>> doc = Document({'hello': 'world'})\n"
    "    >>> print(doc.dumps(flags=WriterFlags.PRETTY))\n"
    "    {\n"
    "        \"hello\": \"world\"\n"
    "    }\n"
    "\n"
    "To dump just part of a document, you can pass a JSON pointer (RFC 6901)\n"
    "as ``at_pointer``, ex:\n"
    "\n"
    ".. doctest::\n"
    "\n"
    "    >>> doc = Document({'results': {'count': 3, 'rows': [55, 66, 77]}})\n"
    "    >>> print(doc.dumps(at_pointer='/results/rows'))\n"
    "    [55,66,77]\n"
    "\n"
    ":param flags: Flags that control JSON writing behaviour.\n"
    ":type flags: :class:`yyjson.WriterFlags`, optional\n"
    ":param at_pointer: An optional JSON pointer specifying what part of the\n"
    "                   document should be dumped. If not specified, defaults\n"
    "                   to the entire Document.\n"
    ":type at_pointer: str, optional\n"
    ":returns: The serialized ``Document``.\n"
    ":rtype: ``str``"
);
static PyObject *
Document_dumps(DocumentObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {
        "flags",
        "at_pointer",
        NULL
    };
    yyjson_write_flag w_flag = 0;
    const char *pointer = NULL;
    Py_ssize_t pointer_size;

    if(!PyArg_ParseTupleAndKeywords(
            args,
            kwds,
            "|$Is#",
            kwlist,
            &w_flag,
            &pointer,
            &pointer_size
        )) {
        return NULL;
    }

    char *result = NULL;
    size_t w_len;
    yyjson_write_err w_err;
    PyObject *obj_result = NULL;

    if (self->i_doc != NULL) {
        yyjson_val *val_to_serialize = NULL;

        if (pointer) {
            val_to_serialize = yyjson_doc_get_pointern(
                self->i_doc,
                pointer,
                pointer_size
            );
        } else {
            val_to_serialize = yyjson_doc_get_root(self->i_doc);
        }

        result = yyjson_val_write_opts(
            val_to_serialize,
            w_flag,
            self->alc,
            &w_len,
            &w_err
        );
    } else {
        yyjson_mut_val *mut_val_to_serialize = NULL;

        if (pointer) {
            mut_val_to_serialize = yyjson_mut_doc_get_pointern(
                self->m_doc,
                pointer,
                pointer_size
            );
        } else {
            mut_val_to_serialize = yyjson_mut_doc_get_root(self->m_doc);
        }

        result = yyjson_mut_val_write_opts(
            mut_val_to_serialize,
            w_flag,
            self->alc,
            &w_len,
            &w_err
        );
    }

    if (yyjson_unlikely(!result)) {
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

    if (self->i_doc) {
        yyjson_val *result = yyjson_doc_get_pointer(
            self->i_doc,
            pointer
        );

        if (!result) {
            PyErr_SetString(PyExc_ValueError, "Not a valid JSON Pointer");
            return NULL;
        }

        return element_to_primitive(result);
    } else {
        yyjson_mut_val *result = yyjson_mut_doc_get_pointer(
            self->m_doc,
            pointer
        );

        if (!result) {
            PyErr_SetString(PyExc_ValueError, "Not a valid JSON Pointer");
            return NULL;
        }

        return mut_element_to_primitive(result);
    }
}

PyDoc_STRVAR(
    Document_merge_patch_doc,
    "Performs an RFC 7386 JSON merge-patch and returns the result as a\n"
    "new Document.\n"
    "\n"
    "If you intend to re-use the same patch many times, you should pre-create\n"
    "a :class:`Document` containing the patch, ex:\n"
    "\n"
    ".. doctest::\n"
    "\n"
    "    >>> patch = Document({'planets': {'pluto': False}})\n"
    "    >>> doc = Document({'planets': {'mars': True, 'pluto': True}})\n"
    "    >>> new_doc = doc.merge_patch(patch)\n"
    "    >>> print(new_doc.dumps())\n"
    "    {\"planets\":{\"mars\":true,\"pluto\":false}}\n"
    "\n"
    ":param patch: The JSON patch to apply.\n"
    ":type patch: Document, dict(), or a string containing a JSON document.\n"
    ":param at_pointer: An optional JSON pointer specifying what part of the\n"
    "                   document should be patched. If not specified, defaults\n"
    "                   to the entire Document.\n"
    ":type at_pointer: str, optional"
);
static PyObject *
Document_merge_patch(DocumentObject *self, PyObject *args, PyObject *kwds)
{
    // Create a new, essentially empty Document which will serve as the
    // container for the patch result.
    DocumentObject *obj = (DocumentObject*)PyObject_CallFunction(
        (PyObject*)&DocumentType,
        "(O)",
        Py_None
    );
    Py_INCREF(Py_None);

    if (!obj) {
        return NULL;
    }

    static char *kwlist[] = {
        "patch",
        "at_pointer",
        NULL
    };

    const char *pointer = NULL;
    Py_ssize_t pointer_size;
    yyjson_mut_val *original = NULL;
    PyObject *patch = NULL;

    if(!PyArg_ParseTupleAndKeywords(
            args,
            kwds,
            "O|$s#",
            kwlist,
            &patch,
            &pointer,
            &pointer_size
        )) {
        return NULL;
    }

    ENSURE_MUTABLE(self);

    // If a pointer was provided, that's the value we're going to be patching,
    // otherwise we use the root of the document.
    if (pointer != NULL) {
        original = yyjson_mut_doc_get_pointern(
            self->m_doc,
            pointer,
            pointer_size
        );
        if (!original) {
            PyErr_SetString(PyExc_ValueError, "Not a valid JSON Pointer");
            return NULL;
        }
    } else {
        original = yyjson_mut_doc_get_root(self->m_doc);
        if (yyjson_unlikely(!original)) {
            PyErr_SetString(PyExc_ValueError, "Document has no root.");
            return NULL;
        }
    }

    yyjson_mut_val *patch_val = NULL;

    if (PyObject_IsInstance(patch, (PyObject*)&DocumentType)) {
        // If we were given an existing document, we'll just take the root
        // and use it as our patch.
        patch_val = yyjson_mut_doc_get_root(((DocumentObject*)patch)->m_doc);
    } else {
        // Otherwise, see if what we were given is a valid JSON document, and
        // use it if so.
        DocumentObject *patch_doc = (DocumentObject*)PyObject_CallFunction(
            (PyObject*)&DocumentType,
            "(O)",
            patch
        );
        if (!patch_doc) return NULL;
        ENSURE_MUTABLE(patch_doc);
        patch_val = yyjson_mut_doc_get_root(patch_doc->m_doc);
    }

    yyjson_mut_val *merged = yyjson_mut_merge_patch(
        obj->m_doc,
        original,
        patch_val
    );

    yyjson_mut_doc_set_root(obj->m_doc, merged);

    return (PyObject*)obj;
}

static Py_ssize_t
Document_length(DocumentObject *self)
{
    if (self->i_doc != NULL) {
        yyjson_val *root = yyjson_doc_get_root(self->i_doc);
        if (yyjson_unlikely(!root)) {
            return 0;
        }

        yyjson_type type = yyjson_get_type(root);
        if (type == YYJSON_TYPE_OBJ) {
            return yyjson_obj_size(root);
        } else if (type == YYJSON_TYPE_ARR) {
            return yyjson_arr_size(root);
        }
    } else {
        yyjson_mut_val *root = yyjson_mut_doc_get_root(self->m_doc);
        if (yyjson_unlikely(!root)) {
            return 0;
        }

        yyjson_type type = yyjson_mut_get_type(root);
        if (type == YYJSON_TYPE_OBJ) {
            return yyjson_mut_obj_size(root);
        } else if (type == YYJSON_TYPE_ARR) {
            return yyjson_mut_arr_size(root);
        }
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
    {"at_pointer",
        (PyCFunction)(void(*)(void))Document_get_pointer,
        METH_VARARGS,
        Document_get_pointer_doc
    },
    {"merge_patch",
        (PyCFunction)(void(*)(void))Document_merge_patch,
        METH_VARARGS | METH_KEYWORDS,
        Document_merge_patch_doc
    },
    {NULL} /* Sentinel */
};

static PyGetSetDef Document_members[] = {
    {"as_obj",
        (getter)Document_as_obj,
        NULL,
        "Converts the Document to a native Python object.",
        NULL
    },
    {NULL} /* Sentinel */
};


static PyMappingMethods Document_mapping_methods = {
    (lenfunc)Document_length,
    NULL,
    NULL
};

PyTypeObject DocumentType = {
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
