"""
Test the shims for compatibility with the standard library JSON module.

We ignore most options.
"""
from io import BytesIO, StringIO

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
