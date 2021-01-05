from yyjson import Document


def test_json_patch(patch_test):
    """Test against the semi-official JSON Patch tests."""
    doc = Document(patch_test['doc'])
    result = doc.patch(patch_test['patch'])
    if 'error' in patch_test:
        # This test was expected to fail.
        assert result is False
    else:
        assert result, 'patch() returned False when True was expected.'
        assert doc.as_obj == patch_test['expected']
