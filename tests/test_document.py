import math

import pytest

from yyjson import Document, WriterFlags, ReaderFlags


def test_document_from_str():
    """Ensure we can parse a document from a str."""
    doc = Document('{"hello": "world"}')
    assert doc.as_obj == {'hello': 'world'}


def test_document_types():
    """Ensure each primitive type can be upcast."""
    values = (
        ('"hello"', 'hello'),
        ('1', 1),
        ('-1', -1),
        ('1.03', 1.03),
        ('true', True),
        ('false', False),
        ('null', None),
        ('{"hello": "world"}', {"hello": "world"}),
        ('[0, 1, 2]', [0, 1, 2])
    )

    for src, dst in values:
        doc = Document(src)
        assert doc.as_obj == dst


def test_document_dumps():
    """
    Ensure we can properly dump an immutable document to a string.
    """
    doc = Document('{"hello": "world"}')

    # Minified by default.
    assert doc.dumps() == '{"hello":"world"}'
    assert doc.dumps(flags=WriterFlags.PRETTY) == (
        '{\n'
        '    "hello": "world"\n'
        '}'
    )

    doc = Document('{}')
    assert doc.dumps() == '{}'

    doc = Document({})
    assert doc.dumps() == '{}'

    doc = Document([])
    assert doc.dumps() == '[]'


def test_document_dumps_nan_and_inf():
    """
    Ensure we can dump documents with Infinity and NaN.
    """
    # In standards mode, NaN & Inf should be a hard error.
    with pytest.raises(ValueError):
        doc = Document('{"hello": NaN}')

    with pytest.raises(ValueError):
        doc = Document('{"hello": Infinity}')

    doc = Document('''{
        "hello": NaN,
        "world": Infinity
    }''', flags=ReaderFlags.ALLOW_INF_AND_NAN)
    obj = doc.as_obj
    assert math.isnan(obj['hello'])
    assert math.isinf(obj['world'])
    
    
def test_document_dumps_raw():
    doc = Document([9_223_372_036_854_775_807 + 1])
    print(doc.dumps())


def test_document_get_pointer():
    """
    Ensure JSON pointers work.
    """
    doc = Document('''{
        "size" : 3,
        "users" : [
            {"id": 1, "name": "Harry"},
            {"id": 2, "name": "Ron"},
            {"id": 3, "name": "Hermione"}
        ]}'''
    )

    assert doc.get_pointer('/size') == 3
    assert doc.get_pointer('/users/0') == {
        'id': 1,
        'name': 'Harry'
    }
    assert doc.get_pointer('/users/1/name') == 'Ron'

    with pytest.raises(ValueError):
        doc.get_pointer('bob')


def test_document_length():
    """
    Ensure we can get the length of mapping types.
    """
    doc = Document('''{"hello": "world"}''')
    assert len(doc) == 1

    doc = Document('''[0, 1, 2]''')
    assert len(doc) == 3

    with pytest.raises(TypeError):
        doc = Document('1')
        len(doc)

    doc = Document({})
    assert len(doc) == 0

    doc = Document([0, 1, 2])
    assert len(doc) == 3
