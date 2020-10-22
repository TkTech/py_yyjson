from yyjson import Document


def test_document_from_str():
    """Ensure we can parse a document from a str."""
    doc = Document('{"hello": "world"}')
    assert doc.as_obj == {'hello': 'world'}


def test_document_size():
    """Ensure we can find the size (in bytes) of a parsed document."""
    doc = Document('{"hello": "world"}')
    assert doc.size == 18


def test_document_count():
    """Ensure we can find the # of elements parsed."""
    doc = Document('{"hello": "world"}')
    assert doc.count == 3


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
