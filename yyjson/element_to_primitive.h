/*
 * Template body for the document-to-Python-object converters. yyjson's
 * mutable API is a 1:1 mirror of the immutable one (identical foreach macro
 * shapes and accessor names, differing only by the `mut_` infix), so the one
 * optimized implementation is instantiated for both by #including this file
 * twice from document.c. No include guard, on purpose.
 *
 * The includer must define:
 *
 *   CONVERT_FN       the function name to generate
 *   CONVERT_VAL      the value type (yyjson_val / yyjson_mut_val)
 *   CONVERT_API(n)   pastes the API prefix (yyjson_##n / yyjson_mut_##n)
 *
 * and have cached_key(), unicode_from_str(), NEW_DICT/DICT_SET_KEYVAL,
 * PY_YYJSON_MAX_DEPTH, and YY_DecimalClass in scope.
 */

/**
 * Recursively convert the given value into an equivalent high-level Python
 * object. `depth` is the current container nesting level.
 **/
static PyObject *CONVERT_FN(CONVERT_VAL *val, int depth) {
  yyjson_type type = CONVERT_API(get_type)(val);

  // Containers are the cold path -- the vast majority of values are scalars.
  // We walk containers directly (via the foreach macros) rather than through
  // the iterator API, which avoids the per-element iterator bookkeeping.
  if (yyjson_unlikely(type == YYJSON_TYPE_ARR || type == YYJSON_TYPE_OBJ)) {
    size_t idx, max;

    if (yyjson_unlikely(depth >= PY_YYJSON_MAX_DEPTH)) {
      PyErr_SetString(
          PyExc_RecursionError,
          "maximum recursion depth exceeded while converting JSON");
      return NULL;
    }
    depth++;

    if (type == YYJSON_TYPE_ARR) {
      CONVERT_VAL *ele;
      PyObject *arr = PyList_New(CONVERT_API(arr_size)(val));
      if (!arr) return NULL;
      CONVERT_API(arr_foreach)(val, idx, max, ele) {
        PyObject *py_val = CONVERT_FN(ele, depth);
        if (!py_val) {
          Py_DECREF(arr);
          return NULL;
        }
        PyList_SET_ITEM(arr, idx, py_val);
      }
      return arr;
    } else {
      CONVERT_VAL *key, *ele;
      PyObject *dict = NEW_DICT(CONVERT_API(obj_size)(val));
      if (!dict) return NULL;
      CONVERT_API(obj_foreach)(val, idx, max, key, ele) {
        PyObject *py_key =
            cached_key(CONVERT_API(get_str)(key), CONVERT_API(get_len)(key));
        if (!py_key) {
          Py_DECREF(dict);
          return NULL;
        }
        PyObject *py_val = CONVERT_FN(ele, depth);
        if (!py_val) {
          Py_DECREF(py_key);
          Py_DECREF(dict);
          return NULL;
        }
        int rc = DICT_SET_KEYVAL(dict, py_key, py_val);
        Py_DECREF(py_key);
        Py_DECREF(py_val);
        if (rc == -1) {
          Py_DECREF(dict);
          return NULL;
        }
      }
      return dict;
    }
  }

  // Scalar hot path.
  switch (type) {
    case YYJSON_TYPE_STR:
      return unicode_from_str(CONVERT_API(get_str)(val),
                              CONVERT_API(get_len)(val));
    case YYJSON_TYPE_NUM:
      switch (CONVERT_API(get_subtype)(val)) {
        case YYJSON_SUBTYPE_UINT:
          return PyLong_FromUnsignedLongLong(CONVERT_API(get_uint)(val));
        case YYJSON_SUBTYPE_SINT:
          return PyLong_FromLongLong(CONVERT_API(get_sint)(val));
        default:  // YYJSON_SUBTYPE_REAL
          return PyFloat_FromDouble(CONVERT_API(get_real)(val));
      }
    case YYJSON_TYPE_BOOL:
      if (CONVERT_API(get_subtype)(val) == YYJSON_SUBTYPE_TRUE) {
        Py_RETURN_TRUE;
      }
      Py_RETURN_FALSE;
    case YYJSON_TYPE_NULL:
      Py_RETURN_NONE;
    case YYJSON_TYPE_RAW: {
      PyObject *result;
      PyObject *uni = unicode_from_str(CONVERT_API(get_raw)(val),
                                       CONVERT_API(get_len)(val));
      if (!uni) return NULL;
      result = PyObject_CallOneArg(YY_DecimalClass, uni);
      Py_DECREF(uni);
      return result;
    }
    default:  // YYJSON_TYPE_NONE
      PyErr_SetString(PyExc_TypeError, "Unknown tape type encountered.");
      return NULL;
  }
}
