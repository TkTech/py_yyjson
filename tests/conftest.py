from pathlib import Path

import pytest

import yyjson


def pytest_addoption(parser):
    parser.addoption(
        '--runslow',
        action='store_true',
        default=False,
        help='run slow tests'
    )


def pytest_configure(config):
    config.addinivalue_line('markers', 'slow: mark test as slow to run')


def pytest_collection_modifyitems(config, items):
    if config.getoption('--runslow'):
        # --runslow given in cli: do not skip slow tests
        return

    skip_slow = pytest.mark.skip(reason='need --runslow option to run')
    for item in items:
        if 'slow' in item.keywords:
            item.add_marker(skip_slow)


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
