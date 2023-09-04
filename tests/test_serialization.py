import pytest

import yyjson


class ClassThatCantBeSerialized:
    pass


def test_serialize_unknown():
    """
    Ensure we raise an appropriate exception when trying to serialize an
    object we can't handle.
    """
    with pytest.raises(TypeError):
        yyjson.Document({
            "example": ClassThatCantBeSerialized()
        })

    with pytest.raises(TypeError):
        yyjson.Document([ClassThatCantBeSerialized()])
