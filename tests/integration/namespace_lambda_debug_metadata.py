"""Inspect CodeView emitted for a captured lambda in a separate namespace source file.

Run only with the authorized compiler; llvm-readobj is resolved by name from PATH.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(command, root):
    command = [str(value) for value in command]
    result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                            errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def main():
    if shutil.which('llvm-readobj') is None:
        print('SKIP: llvm-readobj is unavailable on PATH', flush=True)
        return 77
    compiler = sys.argv[1]
    with tempfile.TemporaryDirectory(prefix='nsc-namespace-lambda-debug-') as temp:
        root = Path(temp)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
                           '<Compile Include="Main.ns"/><Compile Include="Creator.ns"/>'
                           '</ItemGroup></Project>')
        (root / 'Main.ns').write_text(
            'delegate int Read(); class Box { public int Value; public Box(int value) { Value = value; } } '
            'class Program { public static int Main() { return Creator.Work() * 2; } }')
        (root / 'Creator.ns').write_text(
            'namespace Creator { public int Work() { raii Box box = new Box(21); '
            'raii Read action = [move box] () => box.Value; return action(); } }')
        obj = root / 'Probe.obj'
        built = run([compiler, '--emit-project-oop-obj', project, '-O0', '-g', '-o', obj], root)
        if built.returncode:
            return 1
        metadata = run(['llvm-readobj', '--codeview', obj], root)
        if metadata.returncode:
            return 1
        output = metadata.stdout + metadata.stderr
        failures = [text for text in ('Creator.ns', 'box') if text.lower() not in output.lower()]
        print('MISSING', failures, flush=True)
        return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
