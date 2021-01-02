from yyjson import Document


def test_json_patch(patch_test):
    doc = Document(patch_test['doc'])
    result = doc.patch(patch_test['patch'])
    if 'error' in patch_test:
        # This test was expected to fail.
        assert result is False
    else:
        assert result
        assert doc.as_obj == patch_test['expected']
