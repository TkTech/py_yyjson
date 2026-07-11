"""
Tests for JSON Patch (RFC 6902) support.
"""
from pathlib import Path

import pytest

from yyjson import Document


@pytest.mark.parametrize(
    "context",
    [
        {
            "original": '{"baz": "qux", "foo": "bar"}',
            "patch": [
                {"op": "replace", "path": "/baz", "value": "boo"},
                {"op": "add", "path": "/hello", "value": ["world"]},
                {"op": "remove", "path": "/foo"},
            ],
            "modified": {"baz": "boo", "hello": ["world"]},
        },
        {
            "original": {"baz": "qux", "foo": "bar"},
            "patch": [
                {"op": "replace", "path": "/baz", "value": "boo"},
                {"op": "add", "path": "/hello", "value": ["world"]},
                {"op": "remove", "path": "/foo"},
            ],
            "modified": {"baz": "boo", "hello": ["world"]},
        },
    ],
)
def test_json_patch(context):
    """
    Ensures we can do a simple JSON Patch with various combinations of mutable
    and immutable documents.
    """
    original = Document(context["original"])

    patch = Document(context["patch"])

    modified = original.patch(patch)

    assert modified.as_obj == context["modified"]


@pytest.mark.parametrize(
    "self_content",
    [
        {"a": 1, "b": 2},    # mutable Document (built from a Python object)
        '{"a": 1, "b": 2}',  # immutable Document (parsed from JSON text)
    ],
)
def test_patch_coerces_plain_arguments(self_content):
    """
    patch() accepts a plain dict / list / JSON-string patch and coerces it to a
    Document, so callers don't have to wrap it by hand.
    """
    doc = Document(self_content)

    # JSON Patch as a plain list of operations.
    assert doc.patch([{"op": "add", "path": "/c", "value": 3}]).as_obj == {
        "a": 1, "b": 2, "c": 3
    }
    # Merge-Patch as a plain dict (null deletes "b").
    assert doc.patch({"b": None, "c": 3}, use_merge_patch=True).as_obj == {
        "a": 1, "c": 3
    }
    # Merge-Patch given as a JSON string is parsed, not treated as a value.
    assert doc.patch('{"b": 9}', use_merge_patch=True).as_obj == {"a": 1, "b": 9}


@pytest.mark.parametrize(
    "self_content",
    [
        {"a": 1},    # mutable Document
        '{"a": 1}',  # immutable Document
    ],
)
@pytest.mark.parametrize("bad_patch", ["not valid json", 42])
def test_patch_invalid_argument_raises(self_content, bad_patch):
    """
    An argument that can't become a valid patch Document raises cleanly on both
    mutable and immutable documents. The mutable path previously skipped the
    type check and segfaulted on the raw pointer cast.
    """
    doc = Document(self_content)
    with pytest.raises((TypeError, ValueError)):
        doc.patch(bad_patch)


def test_json_patch_samples():
    tests = Document(Path(__file__).parent / "tests.json").as_obj

    for test in tests:
        if test.get("disabled"):
            continue

        original = Document(test["doc"])
        patch = Document(test["patch"])

        try:
            modified = original.patch(patch)
        except Exception as exc:
            # We're _supposed_ to raise an error if there's an error key,
            # but our error messages aren't going to match at all so just
            # let it go.
            if test.get("error"):
                continue

            raise exc

        assert modified.as_obj == test["expected"]

