"""Reproduce original #70 with byte-identical source and project copies."""

import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile


REPOSITORY = Path('A:/nsc-limitaciones-vistas')
ORIGINALS = Path('A:/native-sharp-reference/code-examples/chapter-16/'
                 '70_generic_vector_and_matrix/expected-errors/temporary_in_operator')
EVIDENCE = Path('A:/nsc-vistas-evidencia')
INPUTS = ('Program.ns', 'Rational.ns', 'TemporaryInOperator.nsproj', 'README.md')


def digest(path):
    """Identify the exact bytes of a reproduction input or a tool."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command):
    """Run a compiler action or accepted example from the authorized repository."""
    command = [str(part) for part in command]
    result = subprocess.run(command, cwd=REPOSITORY, capture_output=True, text=True,
                            errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    runtime = Path(os.environ.get('NSC_WINDOWS_RUNTIME_LIBRARY', '')).resolve()
    for name, path in (('compiler', compiler), ('runtime', runtime)):
        if path.drive.casefold() != 'a:' or not path.is_file():
            raise RuntimeError(name + ' must be an existing artifact on A:')
        print(name.upper(), path, 'SHA256', digest(path), flush=True)
    for variable in ('TEMP', 'TMP'):
        location = os.environ.get(variable)
        if not location or Path(location).resolve().drive.casefold() != 'a:':
            raise RuntimeError(variable + ' must identify a directory on A:')
    root = Path(tempfile.mkdtemp(prefix='original-70-vistas-', dir=EVIDENCE))
    print('COPY', root, flush=True)
    hashes = {}
    for name in INPUTS:
        original = ORIGINALS / name
        copied = root / name
        copied.write_bytes(original.read_bytes())
        hashes[name] = digest(original)
        copied_hash = digest(copied)
        print('INPUT', original, 'SHA256', hashes[name], 'COPY-SHA256', copied_hash, flush=True)
        if hashes[name] != copied_hash:
            raise RuntimeError('copy differs from original: ' + name)
    project = root / 'TemporaryInOperator.nsproj'
    failures = []
    checked = run([compiler, '--check-project-method-bodies', project])
    if checked.returncode:
        failures.append(('semantic analysis', checked.returncode))
    else:
        for level in ('O0', 'O2'):
            executable = root / ('original-' + level + '.exe')
            built = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', executable,
                         '--no-artifact-cache', '--no-error-log'])
            if built.returncode or not executable.is_file():
                failures.append((level, 'emission', built.returncode))
                continue
            executed = run([executable])
            if executed.returncode != 0:
                failures.append((level, 'execution', executed.returncode))
    for name, previous in hashes.items():
        if digest(ORIGINALS / name) != previous:
            failures.append((name, 'original changed during probe'))
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
