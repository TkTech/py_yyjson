import enum
from pathlib import Path
from collections import defaultdict

import tabulate
from yyjson import Document


class Status(enum.Enum):
    GOOD = '🎉 expected result'
    SHOULD_HAVE_FAILED = '🔥 parsing should have failed but succeeded'
    SHOULD_HAVE_SUCCEEDED = '🔥 parsing should have succeeded but failed'
    UNDEF_FAILED = '➖ result undefined, parsing failed'
    UNDEF_SUCCEEDED = '➕ result undefined, parsing succeeded'


tests = {
    'y': (Path('jsonexamples') / 'test_parsing').glob('y_*.json'),
    'n': (Path('jsonexamples') / 'test_parsing').glob('n_*.json'),
    'i': (Path('jsonexamples') / 'test_parsing').glob('i_*.json')
}


table = []
summary = defaultdict(int)
for expected_result, files in tests.items():
    for file in files:
        path = f'[{file}]({file})'
        try:
            with open(file, 'rb') as fin:
                Document(fin.read())
        except ValueError:
            if expected_result == 'y':
                result = Status.SHOULD_HAVE_SUCCEEDED.value
                summary[Status.SHOULD_HAVE_SUCCEEDED] += 1
            elif expected_result == 'i':
                result = Status.UNDEF_FAILED.value
                summary[Status.UNDEF_FAILED] += 1
            else:
                result = Status.GOOD.value
                summary[Status.GOOD] += 1
        else:
            if expected_result == 'n':
                result = Status.SHOULD_HAVE_FAILED.value
                summary[Status.SHOULD_HAVE_FAILED] += 1
            elif expected_result == 'i':
                result = Status.UNDEF_SUCCEEDED.value
                summary[Status.UNDEF_SUCCEEDED] += 1
            else:
                result = Status.GOOD.value
                summary[Status.GOOD] += 1

        table.append([path, result])

print(
    '\n'.join([
        '# Minefield results for yyjson',
        '## Summary',
        tabulate.tabulate([
            [summary[Status.GOOD], Status.GOOD.value],
            [
                summary[Status.SHOULD_HAVE_FAILED],
                Status.SHOULD_HAVE_FAILED.value
            ],
            [
                summary[Status.SHOULD_HAVE_SUCCEEDED],
                Status.SHOULD_HAVE_SUCCEEDED.value
            ],
            [summary[Status.UNDEF_FAILED], Status.UNDEF_FAILED.value],
            [summary[Status.UNDEF_SUCCEEDED], Status.UNDEF_SUCCEEDED.value]
        ], headers=['count', 'result'], tablefmt='github'),
        '## Full Results',
        tabulate.tabulate(
            table,
            headers=[
                'file',
                'result'
            ],
            tablefmt='github'
        )
    ])
)
