import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1])
    prefix = '''class Counter { public static int Order; }
struct Guard { public int Id; public Guard(int id) { Id = id; }
 ~Guard() { Counter.Order = Counter.Order * 10 + Id; } }
'''
    cases = {
        'literal': '{ (Guard, int, Guard) pair = (new Guard(1), 0, new Guard(2)); }',
        'aggregate': '{ (Guard, int, Guard) pair = { new Guard(1), 0, new Guard(2) }; }',
        'nested': '{ (Guard, (int, Guard)) pair = (new Guard(1), (0, new Guard(2))); }',
    }
    static_cases = {
        'static_scalar': 'class Holder { public static (int, long) value; }',
        'static_nested': 'class Holder { public static (int, (int, long)) value; }',
        'static_value': 'class Holder { public static (int, Guard) value; }',
        'static_generic_open': 'class Holder<T> { public static (int, T) value; }',
    }
    with tempfile.TemporaryDirectory(prefix='nsc-tuple-value-drop-') as temp:
        root = Path(temp)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        for name, statement in cases.items():
            source = prefix + 'class Program { public static int Main() { ' + statement + \
                ' return Counter.Order == 21 ? 42 : Counter.Order; } }'
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / (name + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-o', str(exe), '-' + level]
                build = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                assert build.returncode == 0, (name, level, build.stdout, build.stderr)
                if os.name == 'nt':
                    run = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                    print(f'{name} {level} exit={run.returncode}', flush=True)
                    assert run.returncode == 42, (name, level, run.returncode, run.stdout, run.stderr)
        for name, declaration in static_cases.items():
            (root / 'Main.ns').write_text(prefix + declaration +
                                         'class Program { public static int Main() { return 42; } }',
                                         encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / (name + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-o', str(exe), '-' + level]
                build = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                       errors='replace', timeout=120)
                assert build.returncode == 0, (name, level, build.stdout, build.stderr)
                if os.name == 'nt':
                    run = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True,
                                         errors='replace', timeout=120)
                    print(f'{name} {level} exit={run.returncode}', flush=True)
                    assert run.returncode == 42, (name, level, run.returncode, run.stdout, run.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
