"""Recheck the unmodified #39 increment and decrement declarations."""

import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile


REPOSITORY = Path(__file__).resolve().parents[2]
ORIGINALS = Path('A:/native-sharp-reference/code-examples/chapter-07/39_matrix_mxn_class/'
                 'operator-catalog/expected-errors')


def digest(path):
    """Identify the exact bytes of an input or tool consumed by the probe."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command):
    """Run a compiler action from the authorized repository and expose its result."""
    command = [str(part) for part in command]
    result = subprocess.run(command, cwd=REPOSITORY, capture_output=True, text=True,
                            errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def validate_environment(compiler):
    """Require the compiler, runtime, and Python temporary paths on A:."""
    if compiler.drive.casefold() != 'a:' or not compiler.is_file():
        raise RuntimeError('compiler must be an existing file on A:')
    runtime_value = os.environ.get('NSC_WINDOWS_RUNTIME_LIBRARY')
    if not runtime_value:
        raise RuntimeError('NSC_WINDOWS_RUNTIME_LIBRARY must identify the isolated runtime')
    runtime = Path(runtime_value).resolve()
    if runtime.drive.casefold() != 'a:' or not runtime.is_file():
        raise RuntimeError('runtime must be an existing file on A:')
    for variable in ('TEMP', 'TMP'):
        location = os.environ.get(variable)
        if not location or Path(location).resolve().drive.casefold() != 'a:':
            raise RuntimeError(variable + ' must identify a directory on A:')
    if Path(tempfile.gettempdir()).resolve().drive.casefold() != 'a:':
        raise RuntimeError('Python temporary directory must be on A:')
    print('COMPILER', compiler, 'SHA256', digest(compiler), flush=True)
    print('RUNTIME', runtime, 'SHA256', digest(runtime), flush=True)


def check_original(compiler, root, filename, failures):
    """Copy original bytes to an explicit project, compare hashes, analyze and emit."""
    original = ORIGINALS / filename
    source = root / filename
    source.write_bytes(original.read_bytes())
    original_hash = digest(original)
    copy_hash = digest(source)
    print('SOURCE', original, 'SHA256', original_hash, 'COPY', source, 'SHA256', copy_hash, flush=True)
    if original_hash != copy_hash:
        failures.append((filename, 'source differs from original'))
        return
    project = root / (filename + '.nsproj')
    project.write_text('<Project><PropertyGroup><Name>Original39</Name></PropertyGroup>'
                       '<ItemGroup><Compile Include="' + filename + '"/></ItemGroup></Project>',
                       encoding='utf-8')
    checked = run([compiler, '--check-project-method-bodies', project, '--no-error-log'])
    if checked.returncode:
        failures.append((filename, 'semantic analysis', checked.returncode))
        return
    for level in ('O0', 'O2'):
        executable = root / (filename + '-' + level + '.exe')
        built = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', executable,
                     '--no-artifact-cache', '--no-error-log'])
        if built.returncode or not executable.is_file():
            failures.append((filename, level, 'emission', built.returncode))
    if digest(original) != original_hash:
        failures.append((filename, 'original changed during probe'))


def main():
    compiler = Path(sys.argv[1]).resolve()
    validate_environment(compiler)
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-vistas-original-39-') as temporary:
        root = Path(temporary)
        for filename in ('increment_operator.ns', 'decrement_operator.ns'):
            check_original(compiler, root, filename, failures)
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
