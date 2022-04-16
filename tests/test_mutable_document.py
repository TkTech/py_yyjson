from yyjson import Document, MutableDocument


def test_document_from_str():
    """Ensure we can create a mutable document from a string"""
    doc = MutableDocument('{"hello": "world"}')
    assert doc.as_obj == {'hello': 'world'}
