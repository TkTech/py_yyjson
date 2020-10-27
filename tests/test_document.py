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


def test_document_patch():
    doc = Document()

    # Sets the root of the document since none
    # yet exists.
    doc.patch({'hello': 'world'})
    assert doc.dumps() == '{"hello":"world"}'
    doc.patch([0, 1, 2])
    assert doc.dumps() == '[0,1,2]'
    doc.patch(1)
    assert doc.dumps() == '1'
    doc.patch(1.5)
    assert doc.dumps() == '1.5'

    # doc = Document('[5]')
    # # Replace the entire matching array.
    # doc.patch([0, 1, 2], '.[]')
    # # Flatten and append to the array.
    # doc.patch([3, 4], '.[@]')
