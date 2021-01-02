from pathlib import Path

import yyjson


def pytest_generate_tests(metafunc):
    # Generate a parameterized test for each entry in tests.json, which is used
    # for JSON Patch.
    if 'patch_test' in metafunc.fixturenames:
        with open(Path(__file__).parent / 'tests.json', 'rb') as tests:
            json_tests = [
                test for test in
                yyjson.loads(tests.read())
                if not test.get('disabled')
            ]

        metafunc.parametrize(
            'patch_test',
            json_tests,
            ids=[r.get('comment') for r in json_tests]
        )
