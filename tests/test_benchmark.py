"""
Benchmarks for the JSON Patch utilities.
"""
import pytest


def jsonpatch_patcher(doc, patch):
    import jsonpatch
    return jsonpatch.JsonPatch(patch()).apply(doc)


def yyjson_patcher(doc, patch):
    import yyjson
    doc = yyjson.Document(doc)
    doc.patch(patch())
    return doc.as_obj


@pytest.fixture(params=[
    ('jsonpatch', jsonpatch_patcher),
    ('yyjson', yyjson_patcher)
])
def patcher(benchmark, request):
    # benchmark.group = request.param[0]
    benchmark.name = request.param[0]
    return request.param[1]


@pytest.mark.slow
def test_benchmark_json_patch(benchmark, patcher):
    """
    Trivial benchmark using an example from the JSON Patch tutorial.
    """
    # The patch is a lambda due to a bug in jsonpatch causing it to modify
    # the `value` list in the second entry, even with in_place=False.
    result = benchmark(patcher, {}, lambda: [
        {'op': 'add', 'path': '/foo', 'value': 'bar'},
        {'op': 'add', 'path': '/baz', 'value': [1, 2, 3]},
        {'op': 'remove', 'path': '/baz/1'},
        {'op': 'replace', 'path': '/baz/0', 'value': 42},
        {'op': 'remove', 'path': '/baz/1'},
    ])

    assert result == {'foo': 'bar', 'baz': [42]}
