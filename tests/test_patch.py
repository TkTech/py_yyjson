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

