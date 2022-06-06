import pytest

from yyjson import Document


def test_merge_patch():
    """
    Ensure basic JSON Merge-Patch functionality works.
    """
    class RandomObject:
        pass

    doc = Document({
        'a': 1,
        'c': 4
    })
    patch = Document({
        'a': 3,
        'b': 2
    })

    # We should be able to use an existing Document
    assert doc.merge_patch(patch).as_obj == {
        'a': 3,
        'b': 2,
        'c': 4
    }
    # We should be able to use a dictionary
    assert doc.merge_patch({'a': 3, 'b': 2}).as_obj == {
        'a': 3,
        'b': 2,
        'c': 4
    }
    # We should be able to use a serialized JSON string
    assert doc.merge_patch('{"a": 3, "b": 2}').as_obj == {
        'a': 3,
        'b': 2,
        'c': 4
    }
    # We should be able to nuke a key
    assert doc.merge_patch({'a': None, 'b': 2}).as_obj == {
        'b': 2,
        'c': 4
    }

    # Trying to patch with an unknown object should fail rather than crash.
    with pytest.raises(TypeError):
        doc.merge_patch(RandomObject)

    doc = Document({
        'a': 1,
        'b': {
            'a': 2,
            'b': 3
        }
    })

    # We should be able to extract and patch just part of a document.
    assert doc.merge_patch({'a': 5}, at_pointer='/b').as_obj == {
        'a': 5,
        'b': 3
    }
