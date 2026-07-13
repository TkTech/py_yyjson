"""
Test the shims for compatibility with the standard library JSON module.

We ignore most options.
"""
import json
from io import BytesIO, StringIO

import pytest

import yyjson


def test_dump():
    """
    Ensure we can dump a document to a string.
    """
    with StringIO() as test:
        yyjson.dump({"a": 1, "b": 2}, test)
        assert test.getvalue() == '{"a":1,"b":2}'


def test_dumps():
    """
    Ensure we can dump a document to a string.
    """
    assert yyjson.dumps({"a": 1, "b": 2}) == '{"a":1,"b":2}'


def test_load():
    """
    Ensure we can load a document from a string.
    """
    with BytesIO(b'{"a":1,"b":2}') as test:
        assert yyjson.load(test) == {"a": 1, "b": 2}


def test_loads():
    """
    Ensure we can load a document from a string.
    """
    assert yyjson.loads('{"a":1,"b":2}') == {"a": 1, "b": 2}


@pytest.mark.parametrize(
    "value, expected",
    [
        ("hello", '"hello"'),   # a str is a JSON string value, not JSON text
        ("123", '"123"'),       # previously silently emitted 123
        ("[1,2]", '"[1,2]"'),   # previously silently emitted [1,2]
        ("3.14", '"3.14"'),     # previously silently emitted 3.14
        ("", '""'),
        (123, "123"),
        (3.14, "3.14"),
        (True, "true"),
        (None, "null"),
        ({"a": "b"}, '{"a":"b"}'),
        (["x", 1, None], '["x",1,null]'),
    ],
)
def test_dumps_serializes_values_like_json(value, expected):
    """
    dumps() must serialize its argument as a Python value, matching the stdlib
    json module -- in particular a str must not be reparsed as JSON text.
    """
    assert yyjson.dumps(value) == expected
    assert yyjson.dumps(value) == json.dumps(value, separators=(",", ":"))


def test_dump_str_value():
    """dump() of a str writes a JSON string, not the reparsed text."""
    with StringIO() as fp:
        yyjson.dump("hello", fp)
        assert fp.getvalue() == '"hello"'


def test_dumps_bytes_not_serializable():
    """bytes is not JSON-serializable, matching stdlib json (not reparsed)."""
    with pytest.raises(TypeError):
        yyjson.dumps(b"hello")
