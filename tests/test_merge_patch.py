from yyjson import Document


def test_merge_patch():
    doc = Document({
        'a': 1,
        'c': 4
    })
    patch = Document({
        'a': 3,
        'b': 2
    })

    print(doc.merge_patch(patch).dumps())
    print(doc.merge_patch({'a': 3, 'b': 2}).dumps())

