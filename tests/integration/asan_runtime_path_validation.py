"""Exercise real ASan runtime validation with isolated copies on Windows."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


def report(message):
    """Show progress and preserve it in the optional directed-test log."""
    print(message, flush=True)
    log = os.environ.get('NSC_DIRECTED_LOG')
    if log:
        with Path(log).open('a', encoding='utf-8') as output:
            output.write(message + '\n')


def invoke(command, root, label):
    """Run a directed command and report continued execution every 25 seconds."""
    report('START ' + label)
    process = subprocess.Popen([str(part) for part in command], cwd=root,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, errors='replace')
    while True:
        try:
            output, _ = process.communicate(timeout=25)
            break
        except subprocess.TimeoutExpired:
            report('RUNNING ' + label)
    result = subprocess.CompletedProcess(command, process.returncode, output)
    report('END ' + label + ' exit=' + str(result.returncode))
    if output:
        report(output)
    return result


def main():
    """Check matching and mismatching runtime contents in O0 and O2."""
    if len(sys.argv) != 4 or os.name != 'nt':
        raise SystemExit('usage on Windows: asan_runtime_path_validation.py <nsc> <clang> <runtime>')
    log = os.environ.get('NSC_DIRECTED_LOG')
    if log:
        Path(log).open('x', encoding='utf-8').close()
    compiler, clang, runtime = map(Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='nsc-asan-path-') as temporary:
        root = Path(temporary)
        matching_dir = root / 'runtime-matching'
        mismatching_dir = root / 'runtime-invalid'
        matching_dir.mkdir()
        mismatching_dir.mkdir()
        data = runtime.read_bytes()
        matching = matching_dir / runtime.name
        mismatching = mismatching_dir / runtime.name
        matching.write_bytes(data)
        altered = bytearray(data)
        altered[-1] ^= 1
        mismatching.write_bytes(altered)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>AsanPath</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                           '</ItemGroup></Project>', encoding='utf-8')
        (root / 'Main.ns').write_text('int Main() { return 42; }', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / ('probe-' + level + '.exe')
            base = [compiler, '--emit-project-oop-exe', project, '-o', executable,
                    '-' + level, '--sanitize-address', '--asan-clang=' + str(clang)]
            valid = invoke(base + ['--asan-runtime=' + str(matching)], root, level + ':matching')
            assert valid.returncode == 0, valid.stdout
            result = invoke([executable], root, level + ':run')
            assert result.returncode == 42, result.stdout
            invalid = invoke(base + ['--asan-runtime=' + str(mismatching)], root, level + ':mismatch')
            assert invalid.returncode != 0 and 'runtime does not match' in invalid.stdout, invalid.stdout
    return 0


if __name__ == '__main__':
    sys.exit(main())
