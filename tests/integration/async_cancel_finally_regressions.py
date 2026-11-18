"""Run the focused cancellation/finally regressions with isolated output."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


RUNNERS = (
    'async_yield_mixed_cancellation.py',
    'async_cancel_value_cleanup.py',
    'async_cancel_reference_cleanup.py',
    'async_cancel_scalar_types.py',
    'async_cancel_wrapper_cleanup.py',
    'async_cancel_finally_continuation.py',
    'async_cancel_finally_await_exception.py',
)


def main():
    if os.name != 'nt':
        return 77
    if len(sys.argv) != 2:
        raise SystemExit('expected the compiler path')
    compiler = Path(sys.argv[1]).resolve()
    temporary = Path(tempfile.gettempdir()).resolve()
    if compiler.anchor.lower() != 'a:\\' or temporary.anchor.lower() != 'a:\\':
        print('this focused regression requires the isolated compiler and temporary directory on A:')
        return 77
    tests = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix='nsc-cancel-finally-', dir=temporary) as directory:
        for name in RUNNERS:
            output = Path(directory) / Path(name).stem
            command = [sys.executable, '-B', str(tests / name), str(compiler), str(output)]
            print('COMMAND', subprocess.list2cmdline(command), flush=True)
            result = subprocess.run(command, cwd=tests.parent.parent, capture_output=True, text=True,
                                    errors='replace', timeout=180)
            print(result.stdout, end='', flush=True)
            if result.stderr:
                print(result.stderr, end='', file=sys.stderr, flush=True)
            if result.returncode != 0:
                return result.returncode
    return 0


if __name__ == '__main__':
    sys.exit(main())
