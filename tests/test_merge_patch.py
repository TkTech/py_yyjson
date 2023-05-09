"""
Tests for JSON Merge-Patch (RFC 7386) support.
"""
import pytest

from yyjson import Document


@pytest.mark.parametrize(
    "context",
    [
        {
            "original": '{"hello": "earth", "goodbye": "moon"}',
            "patch": '{"hello": "mars"}',
            "modified": {"hello": "mars", "goodbye": "moon"},
        },
        {
            "original": '{"content": {"hello": "earth", "goodbye": "moon"}}',
            "patch": '{"hello": "mars"}',
            "modified": {"hello": "mars", "goodbye": "moon"},
            "at_pointer": "/content",
        },
        {
            "original": {"hello": "earth", "goodbye": "moon"},
            "patch": {"hello": "mars"},
            "modified": {"hello": "mars", "goodbye": "moon"},
        },
        {
            "original": {"content": {"hello": "earth", "goodbye": "moon"}},
            "patch": {"hello": "mars"},
            "modified": {"hello": "mars", "goodbye": "moon"},
            "at_pointer": "/content",
        },
        {
            "original": {"hello": "earth", "goodbye": "moon"},
            "patch": '{"hello": "mars"}',
            "modified": {"hello": "mars", "goodbye": "moon"},
        },
        {
            "original": '{"hello": "earth", "goodbye": "moon"}',
            "patch": {"hello": "mars"},
            "modified": {"hello": "mars", "goodbye": "moon"},
        },
    ],
)
def test_merge_patch(context):
    """
    Ensures we can do a simple JSON Merge-Patch with various combinations of
    mutable and immutable documents.
    """
    original = Document(context["original"])
    patch = Document(context["patch"])

    modified = original.patch(
        patch, at_pointer=context.get("at_pointer"), use_merge_patch=True
    )

    assert modified.as_obj == context["modified"]
