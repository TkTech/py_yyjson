import pytest

from yyjson import Document


@pytest.mark.parametrize('context', [{
    'original': '{"hello": "earth", "goodbye": "moon"}',
    'patch': '{"hello": "mars"}',
    'modified': {'hello': 'mars', 'goodbye': 'moon'}
}, {
    'original': '{"content": {"hello": "earth", "goodbye": "moon"}}',
    'patch': '{"hello": "mars"}',
    'modified': {'hello': 'mars', 'goodbye': 'moon'},
    'at_pointer': '/content'
}, {
    'original': {'hello': 'earth', 'goodbye': 'moon'},
    'patch': {'hello': 'mars'},
    'modified': {'hello': 'mars', 'goodbye': 'moon'}
}, {
    'original': {'content': {'hello': 'earth', 'goodbye': 'moon'}},
    'patch': {'hello': 'mars'},
    'modified': {'hello': 'mars', 'goodbye': 'moon'},
    'at_pointer': '/content'
}, {
    'original': {'hello': 'earth', 'goodbye': 'moon'},
    'patch': '{"hello": "mars"}',
    'modified': {'hello': 'mars', 'goodbye': 'moon'}
}, {
    'original': '{"hello": "earth", "goodbye": "moon"}',
    'patch': {'hello': 'mars'},
    'modified': {'hello': 'mars', 'goodbye': 'moon'}
}])
def test_merge_patch(context):
    original = Document(context['original'])
    patch = Document(context['patch'])

    modified = original.merge_patch(
        patch,
        at_pointer=context.get('at_pointer')
    )

    assert modified.as_obj == context["modified"]