"""Check SIMD mask indexing with the actual Windows compiler."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


PREFIX = '''class Program { public static int Main() {
    simd<int, 4> a = default(simd<int, 4>);
    simd<int, 4> b = default(simd<int, 4>);
    unsafe("prepare SIMD lanes in isolated test") {
        int* input = reinterpret_cast<int*>(&b);
        input[1] = 1; input[3] = 1;
    }
    mask<int, 4> m = a == b;
'''


def invoke(command, directory):
    result = subprocess.run([str(part) for part in command], cwd=directory,
                            capture_output=True, text=True, errors='replace')
    output = result.stdout + result.stderr
    print('COMMAND', *command, 'EXIT', result.returncode, flush=True)
    if output:
        print(output, flush=True)
    return result


def project(root, name, statement, prefix=PREFIX):
    directory = root / name
    directory.mkdir()
    descriptor = directory / 'Probe.nsproj'
    descriptor.write_text('<Project><PropertyGroup><Name>' + name + '</Name>'
                          '</PropertyGroup><ItemGroup><Compile Include="Main.ns"/>'
                          '</ItemGroup></Project>', encoding='utf-8')
    (directory / 'Main.ns').write_text(prefix + statement + '\n} }', encoding='utf-8')
    return descriptor


def main():
    if os.name != 'nt' or len(sys.argv) != 2:
        raise SystemExit('usage: simd_mask_index.py <Windows compiler>')
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-89-mask-index-') as temporary:
        root = Path(temporary)
        valid = project(root, 'Mixed', '''int i = 0;
            if (m[0] && !m[1] && m[2] && !m[3] && m[i++] && !m[i++] && i == 2) return 42;
            return 1;''')
        for level in ('O0', 'O2'):
            executable = valid.parent / (level + '.exe')
            built = invoke([compiler, '--emit-project-oop-exe', valid, '-o', executable,
                            '--no-artifact-cache', '-' + level], root)
            assert built.returncode == 0, built.stdout + built.stderr
            result = invoke([executable], root)
            assert result.returncode == 42, result.stdout + result.stderr

        side_effect_prefix = PREFIX.replace('class Program { public static int Main()',
            'class Program { public static int calls = 0; '
            'public static mask<int, 4> Once(mask<int, 4> value) '
            '{ calls = calls + 1; return value; } public static int Main()')
        receiver = project(root, 'ReceiverOnce',
            'if (Once(m)[0] && calls == 1) return 42; return 1;', side_effect_prefix)
        for level in ('O0', 'O2'):
            executable = receiver.parent / (level + '.exe')
            built = invoke([compiler, '--emit-project-oop-exe', receiver, '-o', executable,
                            '--no-artifact-cache', '-' + level], root)
            assert built.returncode == 0, built.stdout + built.stderr
            result = invoke([executable], root)
            assert result.returncode == 42, result.stdout + result.stderr

        for lane, count in (('long', 2), ('int', 8), ('byte', 16),
                            ('float', 4), ('double', 4)):
            prefix = ('class Program { public static int Main() { '
                      'simd<' + lane + ', ' + str(count) + '> a = '
                      'default(simd<' + lane + ', ' + str(count) + '>); '
                      'mask<' + lane + ', ' + str(count) + '> yes = a == a; '
                      'mask<' + lane + ', ' + str(count) + '> no = a != a; ')
            last = str(count - 1)
            statement = ('if (yes[0] && yes[' + last + '] && !no[0] && !no[' + last +
                         ']) return 42; return 1;')
            descriptor = project(root, 'Count' + lane, statement, prefix)
            for level in ('O0', 'O2'):
                executable = descriptor.parent / (level + '.exe')
                built = invoke([compiler, '--emit-project-oop-exe', descriptor, '-o', executable,
                                '--no-artifact-cache', '-' + level], root)
                assert built.returncode == 0, built.stdout + built.stderr
                result = invoke([executable], root)
                assert result.returncode == 42, result.stdout + result.stderr

        for name, statement, diagnostic in (
                ('NegativeConstant', 'bool value = m[-1]; return 0;', 'constant SIMD mask index'),
                ('HighConstant', 'bool value = m[4]; return 0;', 'constant SIMD mask index'),
                ('WrongType', 'bool value = m[true]; return 0;', 'one integral index'),
                ('TwoIndices', 'bool value = m[0, 1]; return 0;', 'one integral index'),
                ('FromEnd', 'bool value = m[^1]; return 0;', 'one integral index'),
                ('AddressLane', 'unsafe("no lane address") { bool* p = &m[0]; } return 0;', 'address'),
                ('WriteLane', 'm[0] = false; return 0;', 'assign')):
            descriptor = project(root, name, statement)
            result = invoke([compiler, '--check-project-method-bodies', descriptor], root)
            assert result.returncode != 0 and diagnostic in result.stdout + result.stderr
        for name, declaration in (('NegativeDynamic', 'int index = -1;'),
                                  ('HighDynamic', 'int index = 4;'),
                                  ('WideDynamic', 'u128 index = ((u128)1 << 64) + (u128)1;')):
            descriptor = project(root, name,
                declaration + ' bool value = m[index]; return value ? 42 : 43;')
            ir = invoke([compiler, '--dump-project-oop-llvm', descriptor, '-O0'], root)
            assert ir.returncode == 0, ir.stdout + ir.stderr
            assert 'mask.index.inrange' in ir.stdout and 'extractelement' in ir.stdout
            assert '@__nativesharp_trap(i32 4)' in ir.stdout
            for level in ('O0', 'O2'):
                executable = descriptor.parent / (level + '.exe')
                built = invoke([compiler, '--emit-project-oop-exe', descriptor, '-o', executable,
                                '--no-artifact-cache', '-' + level], root)
                assert built.returncode == 0, built.stdout + built.stderr
                result = invoke([executable], root)
                assert result.returncode == 132, result.stdout + result.stderr
        print('simd_mask_index: PASS', flush=True)


if __name__ == '__main__':
    main()
