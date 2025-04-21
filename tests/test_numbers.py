import math
import decimal

import pytest

from yyjson import Document, ReaderFlags, WriterFlags


test_numbers = [
    1,
    2,
    -1,
    -2,
    1.0,
    2.0,
    -1.0,
    -2.0,
    2**63,
    -2**63,
    -2**63-1,
    2**64,
    -2**64,
    2**64+1,
    2**128,
    2**128+1,
    -2**128,
    -2**128-1,
]


def is_bignum(num):
    return -2**63 <= num < 2**64


def test_big_numbers():
    """
    Test the round-tripping of big numbers.

    The test set is from:
        https://blog.trl.sn/blog/what-is-a-json-number/#python-3-8-1
    """
    test_str_numbers = [
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

    for num in test_str_numbers:
        deserialized = Document(num, flags=ReaderFlags.NUMBERS_AS_DECIMAL)

        obj = deserialized.as_obj

        assert str(obj) == num
        assert Document(obj).dumps() == num

def test_numbers_no_flags():
    '''
    Verify expected behavior with no flags - big numbers are converted to
    python floats.
    '''
    for num in test_numbers:
        obj = Document(num).dumps()
        assert str(num) == obj

        val = Document(obj).as_obj
        if is_bignum(num):
            assert val == num
            assert isinstance(val, (int, float))
        else:
            assert isinstance(val, float)


def test_numbers_as_raw():
    '''
    Verify expected behavior of NUMBERS_AS_RAW - all numbers are deserialized
    as python ints/floats.
    '''
    for num in test_numbers:
        obj = Document(num).dumps()
        assert str(num) == obj

        val = Document(obj, flags=ReaderFlags.NUMBERS_AS_RAW).as_obj
        assert val == num
        assert not isinstance(val, decimal.Decimal)


def test_numbers_as_decimal():
    '''
    Verify expected behavior of NUMBERS_AS_DECIMAL - all numbers are
    deserialized as python decimal.Decimals.
    '''
    for num in test_numbers:
        obj = Document(num).dumps()
        assert str(num) == obj

        val = Document(obj, flags=ReaderFlags.NUMBERS_AS_DECIMAL).as_obj
        assert val == num
        assert isinstance(val, decimal.Decimal)


def test_big_numbers_as_raw():
    '''
    Verify expected behavior of BIGNUM_AS_RAW - numbers GE 2**64 and LT -2**63
    are deserialized as python ints/floats.
    '''
    for num in test_numbers:
        obj = Document(num).dumps()
        assert str(num) == obj

        val = Document(obj, flags=ReaderFlags.BIGNUM_AS_RAW).as_obj
        assert val == num
        assert not isinstance(val, decimal.Decimal)


def test_big_numbers_as_decimal():
    '''
    Verify expected behavior of BIGNUM_AS_DECIMAL - numbers GE 2**64 and LT
    -2**63 are deserialized as python decimal.Decimals.
    '''
    for num in test_numbers:
        obj = Document(num).dumps()
        assert str(num) == obj

        val = Document(obj, flags=ReaderFlags.BIG_NUMBERS_AS_DECIMAL).as_obj
        assert val == num
        if is_bignum(num):
            assert not isinstance(val, decimal.Decimal)
        else:
            assert isinstance(val, decimal.Decimal)


def test_float_inf_nan():
    '''
    Verify expected behavior of deserializing Infinity, -Infinity, and NaN
    special values.
    '''
    inf = float('inf')
    ninf = float('-inf')
    nan = float('nan')

    mesg = 'nan or inf number is not allowed'
    with pytest.raises(ValueError) as exc:
        Document([inf]).dumps()
    assert exc.type is ValueError
    assert exc.value.args[0] == mesg

    mesg = 'nan or inf number is not allowed'
    with pytest.raises(ValueError) as exc:
        Document([ninf]).dumps()
    assert exc.type is ValueError
    assert exc.value.args[0] == mesg

    with pytest.raises(ValueError) as exc:
        Document([nan]).dumps()
    assert exc.type is ValueError
    assert exc.value.args[0] == mesg

    obj = Document([inf, ninf, nan]).dumps(flags=WriterFlags.ALLOW_INF_AND_NAN)
    assert obj == '[Infinity,-Infinity,NaN]'

    mesg = 'unexpected character, expected a valid JSON value'
    with pytest.raises(ValueError) as exc:
        Document(obj).as_obj
    assert exc.type is ValueError
    assert exc.value.args[0] == mesg

    for flags in (
        ReaderFlags.ALLOW_INF_AND_NAN,
        ReaderFlags.ALLOW_INF_AND_NAN | ReaderFlags.BIGNUM_AS_RAW,
        ReaderFlags.ALLOW_INF_AND_NAN | ReaderFlags.NUMBERS_AS_RAW,
        ReaderFlags.ALLOW_INF_AND_NAN | ReaderFlags.BIG_NUMBERS_AS_DECIMAL,
    ):
        val = Document(obj, flags=flags).as_obj
        assert isinstance(val, list)
        assert len(val) == 3
        assert isinstance(val[0], float)
        assert val[0] == inf
        assert isinstance(val[1], float)
        assert val[1] == ninf
        assert isinstance(val[2], float)
        assert math.isnan(val[2])

    flags = ReaderFlags.ALLOW_INF_AND_NAN | ReaderFlags.NUMBERS_AS_DECIMAL
    val = Document(obj, flags=flags).as_obj
    assert isinstance(val, list)
    assert len(val) == 3
    assert isinstance(val[0], decimal.Decimal)
    assert val[0] == decimal.Decimal(inf)
    assert isinstance(val[1], decimal.Decimal)
    assert val[1] == decimal.Decimal(ninf)
    assert isinstance(val[2], decimal.Decimal)
    assert math.isnan(val[2])
