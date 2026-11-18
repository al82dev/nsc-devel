"""Compile the original matrix with an actual generic Multiply call."""
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1]).resolve()
    project = Path(__file__).with_name('generic_owner_matrix') / 'GenericOwnerMatrix.nsproj'
    with tempfile.TemporaryDirectory(prefix='nsc-owner-matrix-') as temporary:
        for level in ('O0', 'O2'):
            exe = Path(temporary) / ('matrix-' + level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(exe), '--no-artifact-cache', '--no-error-log']
            result = subprocess.run(command, capture_output=True, text=True, errors='replace', timeout=120)
            print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                  result.stdout + result.stderr, flush=True)
            if result.returncode:
                return 1
            result = subprocess.run([str(exe)], capture_output=True, text=True, errors='replace', timeout=20)
            print('EXEC', exe, 'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
            if result.returncode != 42:
                return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
