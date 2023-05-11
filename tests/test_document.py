import math

import pytest

from yyjson import Document, WriterFlags, ReaderFlags

# The maximum value of a signed 64 bit value.
LLONG_MAX = 9223372036854775807
# The maximum value of an unsigned 64 bit value.
ULLONG_MAX = 18446744073709551615


def test_document_is_mutable():
    """Ensure we can determine if a document is mutable."""
    doc = Document({"hello": "world"})
    assert doc.is_thawed is True

    doc = Document('{"hello": "world"}')
    assert doc.is_thawed is False


def test_document_from_str():
    """Ensure we can parse a document from a str."""
    doc = Document('{"hello": "world"}')
    assert doc.as_obj == {"hello": "world"}


def test_document_types():
    """Ensure each primitive type can be upcast (which does not have its own
    dedicated test.)"""
    values = (
        ('"hello"', "hello"),
        ("1", 1),
        ("-1", -1),
        ('{"hello": "world"}', {"hello": "world"}),
        ("[0, 1, 2]", [0, 1, 2]),
    )

    for src, dst in values:
        doc = Document(src)
        assert doc.as_obj == dst


def test_document_dumps():
    """
    Ensure we can properly dump a document to a string.
    """
    doc = Document('{"hello": "world"}')

    # Minified by default.
    assert doc.dumps() == '{"hello":"world"}'
    assert doc.dumps(flags=WriterFlags.PRETTY) == ("{\n" '    "hello": "world"\n' "}")

    doc = Document("{}")
    assert doc.dumps() == "{}"

    doc = Document({})
    assert doc.dumps() == "{}"

    doc = Document([])
    assert doc.dumps() == "[]"

    doc = Document({"hello": {"there": [0, 1, 2]}})
    assert doc.dumps(at_pointer="/hello/there") == "[0,1,2]"


def test_document_dumps_nan_and_inf():
    """
    Ensure we can dump documents with Infinity and NaN.
    """
    # In standards mode, NaN & Inf should be a hard error.
    with pytest.raises(ValueError):
        Document('{"hello": NaN}')

    with pytest.raises(ValueError):
        Document('{"hello": Infinity}')

    doc = Document(
        """{
        "hello": NaN,
        "world": Infinity
    }""",
        flags=ReaderFlags.ALLOW_INF_AND_NAN,
    )
    obj = doc.as_obj
    assert math.isnan(obj["hello"])
    assert math.isinf(obj["world"])


def test_document_raw_type():
    """
    Ensure we can dump objects that contain integers of any size. This is
    against the JSON specification, but the same as the built-in JSON module.

    In YYJSON, these values are stored in yyjson_raw, which essentially just
    points to the value as a string and does not attempt to interpret it as a
    number.
    """
    # Ensure the maximum yyjson_sint value can be stored.
    doc = Document([LLONG_MAX])
    assert doc.dumps() == "[9223372036854775807]"

    # Ensure the maximum yyjson_sint value + 1 can be stored as a yyjson_uint.
    doc = Document([LLONG_MAX + 1])
    assert doc.dumps() == "[9223372036854775808]"

    # Ensure the maximum yyjson_uint value can be stored.
    doc = Document([ULLONG_MAX])
    assert doc.dumps() == "[18446744073709551615]"

    # Ensure the maximum yyjson_uint value + 1 can be stored as a yyjson_raw.
    doc = Document([ULLONG_MAX + 1])
    assert doc.dumps() == "[18446744073709551616]"
    assert doc.as_obj == [ULLONG_MAX + 1]

    # Ensure we can parse a document with and without the RAW flags set.
    doc = Document("[18446744073709551616000000000]", flags=ReaderFlags.NUMBERS_AS_RAW)
    assert doc.dumps() == "[18446744073709551616000000000]"
    assert doc.as_obj == [18446744073709551616000000000]


def test_document_float_type():
    """
    Ensure we can load and dump floats.
    """
    doc = Document([1.25])
    assert doc.dumps() == "[1.25]"
    assert doc.as_obj == [1.25]

    doc = Document("1.25")
    assert doc.dumps() == "1.25"
    assert doc.as_obj == 1.25


def test_document_boolean_type():
    """
    Ensure we can load and dump boolean types.
    """
    doc = Document("true")
    assert doc.dumps() == "true"
    assert doc.as_obj is True

    doc = Document("false")
    assert doc.dumps() == "false"
    assert doc.as_obj is False

    doc = Document([True])
    assert doc.dumps() == "[true]"
    assert doc.as_obj == [True]

    doc = Document([False])
    assert doc.dumps() == "[false]"
    assert doc.as_obj == [False]


def test_document_none_type():
    """
    Ensure we can load and dump the None type.
    """
    doc = Document("null")
    assert doc.dumps() == "null"
    assert doc.as_obj is None

    doc = Document([None])
    assert doc.dumps() == "[null]"
    assert doc.as_obj == [None]


def test_document_get_pointer():
    """
    Ensure JSON pointers work.
    """
    doc = Document(
        """{
        "size" : 3,
        "users" : [
            {"id": 1, "name": "Harry"},
            {"id": 2, "name": "Ron"},
            {"id": 3, "name": "Hermione"}
        ]}
    """
    )

    assert doc.get_pointer("/size") == 3
    assert doc.get_pointer("/users/0") == {"id": 1, "name": "Harry"}
    assert doc.get_pointer("/users/1/name") == "Ron"

    with pytest.raises(ValueError) as exc:
        doc.get_pointer("bob")

    assert "no prefix" in str(exc.value)

    doc = Document(
        {
            "size": 3,
            "users": [
                {"id": 1, "name": "Harry"},
                {"id": 2, "name": "Ron"},
                {"id": 3, "name": "Hermione"},
            ],
        }
    )

    assert doc.get_pointer("/size") == 3
    assert doc.get_pointer("/users/0") == {"id": 1, "name": "Harry"}
    assert doc.get_pointer("/users/1/name") == "Ron"

    with pytest.raises(ValueError):
        doc.get_pointer("bob")


def test_document_length():
    """
    Ensure we can get the length of mapping types.
    """
    doc = Document("""{"hello": "world"}""")
    assert len(doc) == 1

    doc = Document("""[0, 1, 2]""")
    assert len(doc) == 3

    doc = Document("1")
    assert len(doc) == 0

    doc = Document({})
    assert len(doc) == 0

    doc = Document([0, 1, 2])
    assert len(doc) == 3


def test_document_freeze():
    """
    Ensure we can freeze mutable documents.
    """
    # Documents created from Python objects are always mutable by default,
    # so use that as our starting point.
    doc = Document({"hello": "world"})
    assert doc.is_thawed is True

    doc.freeze()
    assert doc.is_thawed is False
