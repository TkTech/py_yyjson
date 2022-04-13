import pytest

from yyjson import Document


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


def test_new_document():
    Document()


def test_document_dumps():
    doc = Document('{"hello": "world"}')

    # Minified by default.
    assert doc.dumps() == '{"hello":"world"}'
    assert doc.dumps(pretty_print=True) == (
        '{\n'
        '    "hello": "world"\n'
        '}'
    )


def test_document_get_pointer():
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


def test_document_is_mutable():
    doc = Document('''{"hello": "world"}''')
    assert doc.is_mutable is False
    doc = Document()
    assert doc.is_mutable is True