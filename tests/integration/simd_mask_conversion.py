"""Run mixed-lane mask conversions through the isolated compiler."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


def invoke(command, directory, expected_success=True, diagnostic=None):
    result = subprocess.run([str(part) for part in command], cwd=directory,
                            capture_output=True, text=True, errors='replace')
    output = result.stdout + result.stderr
    print('COMMAND', *command, 'EXIT', result.returncode, flush=True)
    if output:
        print(output, flush=True)
    assert (result.returncode == 0) == expected_success, output
    if diagnostic is not None:
        assert diagnostic in output, output
    return result


def project(root, name, body):
    directory = root / name
    directory.mkdir()
    descriptor = directory / 'Probe.nsproj'
    descriptor.write_text('<Project><PropertyGroup><Name>' + name + '</Name>'
                          '</PropertyGroup><ItemGroup><Compile Include="Main.ns"/>'
                          '</ItemGroup></Project>', encoding='utf-8')
    (directory / 'Main.ns').write_text(body, encoding='utf-8')
    return descriptor


def main():
    if os.name != 'nt' or len(sys.argv) != 2:
        raise SystemExit('usage: simd_mask_conversion.py <Windows compiler>')
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-89-mask-convert-') as temporary:
        root = Path(temporary)
        for lane, observed, input_type, one in (
                ('sbyte', 'sbyte', 'sbyte', '1'), ('byte', 'sbyte', 'byte', '1'),
                ('short', 'short', 'short', '1'), ('ushort', 'short', 'ushort', '1'),
                ('int', 'int', 'int', '1'), ('uint', 'int', 'uint', '1'),
                ('long', 'long', 'long', '1'), ('ulong', 'long', 'ulong', '1'),
                ('float', 'int', 'int', '1065353216'),
                ('double', 'long', 'long', '4607182418800017408')):
            body = ('''class Program { public static int Main() {
                simd<LANE, 4> a = default(simd<LANE, 4>);
                simd<LANE, 4> b = default(simd<LANE, 4>);
                unsafe("inspect lanes in isolated regression") {
                    INPUT* input = reinterpret_cast<INPUT*>(&b);
                    input[1] = ONE; input[3] = ONE;
                    mask<LANE, 4> m = a == b;
                    simd<LANE, 4> v = (simd<LANE, 4>)m;
                    OBSERVED* values = reinterpret_cast<OBSERVED*>(&v);
                    if (values[0] == -1 && values[1] == 0 &&
                        values[2] == -1 && values[3] == 0) { return 42; }
                    return 1;
                } } }''').replace('LANE', lane).replace('OBSERVED', observed).replace(
                    'INPUT', input_type).replace('ONE', one)
            descriptor = project(root, 'Mixed' + lane, body)
            for level in ('O0', 'O2'):
                executable = descriptor.parent / (level + '.exe')
                invoke([compiler, '--emit-project-oop-exe', descriptor, '-o', executable,
                        '--no-artifact-cache', '-' + level], root)
                result = subprocess.run([str(executable)], cwd=root)
                print('RUN', lane, level, result.returncode, flush=True)
                assert result.returncode == 42

        for name, source_type, target_type in (
                ('WrongType', 'int, 4', 'long, 4'),
                ('WrongCount', 'int, 4', 'int, 2'),
                ('FloatWrongType', 'float, 4', 'int, 4')):
            body = ('class Program { public static int Main() { '
                    'simd<' + source_type + '> a = default(simd<' + source_type + '>); '
                    'mask<' + source_type + '> m = a == a; '
                    'simd<' + target_type + '> v = (simd<' + target_type + '>)m; '
                    'return 42; } }')
            descriptor = project(root, name, body)
            for level in ('O0', 'O2'):
                invoke([compiler, '--emit-project-oop-exe', descriptor, '-o',
                        descriptor.parent / (level + '.exe'), '--no-artifact-cache',
                        '-' + level], root, expected_success=False,
                       diagnostic='unsupported Stage 34 LLVM conversion')
        print('simd_mask_conversion: PASS', flush=True)


if __name__ == '__main__':
    main()
