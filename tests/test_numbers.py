import sys

from yyjson import Document, ReaderFlags


def test_big_numbers():
    """
    Test the round-tripping of big numbers.

    The test set is from:
        https://blog.trl.sn/blog/what-is-a-json-number/#python-3-8-1
    """
    test_numbers = [
        "10",
        "1000000000",
        "10000000000000001",
        "100000000000000000001",
        "1" + "0" * 4301,
        "10.0",
        "10000000000000001.1",
        "1." + "1" * 34,
        "1E+2",
        "1E+309",
    ]

    for num in test_numbers:
        deserialized = Document(num, flags=ReaderFlags.NUMBERS_AS_RAW)

        obj = deserialized.as_obj

        assert str(obj) == num
        assert Document(obj).dumps() == num
