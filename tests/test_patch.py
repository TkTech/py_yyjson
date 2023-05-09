"""
Tests for JSON Patch (RFC 6902) support.
"""
import pytest

from yyjson import Document


@pytest.mark.parametrize('context', [{
    'original': '{"baz": "qux", "foo": "bar"}',
    'patch': """[
        {"op": "replace", "path": "/baz", "value": "boo"},
        {"op": "add", "path": "/hello", "value": ["world"]},
        {"op": "remove", "path": "/foo"}
    ]""",
    'modified': {
        "baz": "boo",
        "hello": ["world"]
    }
}, {
    'original': {"baz": "qux", "foo": "bar"},
    'patch': [
        {"op": "replace", "path": "/baz", "value": "boo"},
        {"op": "add", "path": "/hello", "value": ["world"]},
        {"op": "remove", "path": "/foo"}
    ],
    'modified': {
        "baz": "boo",
        "hello": ["world"]
    }
}])
def test_json_patch(context):
    """
    Ensures we can do a simple JSON Patch with various combinations of mutable
    and immutable documents.
    """
    original = Document(context["original"])

    patch = Document(context["patch"])

    modified = original.patch(patch)

    assert modified.as_obj == context["modified"]
