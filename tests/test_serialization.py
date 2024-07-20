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
        yyjson.Document({"example": ClassThatCantBeSerialized()})

    with pytest.raises(TypeError):
        yyjson.Document([ClassThatCantBeSerialized()])


def test_serialize_default_func():
    """
    Ensure that we can serialize an object by providing a default function.
    """

    def default(obj):
        if isinstance(obj, ClassThatCantBeSerialized):
            return "I'm a string now!"
        raise TypeError(f"Can't serialize {obj}")

    doc = yyjson.Document(
        {"example": ClassThatCantBeSerialized()}, default=default
    )
    assert doc.as_obj["example"] == "I'm a string now!"
