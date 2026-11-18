"""Captured lambdas created by source namespace functions."""
from pathlib import Path
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
    compiler = Path(sys.argv[1]).resolve()
    head = ('delegate int Read(); class State { public static int Drops; } '
            'class Box { public int Value; public Box(int value) { Value = value; } '
            '~Box() { State.Drops = State.Drops + 1; } } ')
    cases = {
        'copy': ('namespace Helpers { public int Work() { int n = 21; '
                 'raii Read read = [copy n] () => n; return read() + read(); } }',
                 'return Helpers.Work();', True),
        'ref': ('namespace Helpers { public int Work() { int n = 21; '
                'raii Read read = [ref n] () => n; return read() + read(); } }',
                'return Helpers.Work();', True),
        'move': ('namespace Helpers { public int Work() { raii Box box = new Box(21); '
                 'raii Read read = [move box] () => box.Value; return read(); } }',
                 'State.Drops = 0; int value = Helpers.Work(); return value == 21 && State.Drops == 1 ? 42 : 1;', True),
        'move_parameter': ('namespace Helpers { public int Work(owned Box box) { '
                           'raii Read read = [move box] () => box.Value; return read(); } }',
                           'State.Drops = 0; raii Box box = new Box(21); int value = Helpers.Work(move(box)); '
                           'return value == 21 && State.Drops == 1 ? 42 : 1;', True),
        'multiple_functions': ('namespace Helpers { public int First() { raii Box box = new Box(10); '
                               'raii Read read = [move box] () => box.Value; return read(); } '
                               'public int Second() { raii Box box = new Box(11); '
                               'raii Read read = [move box] () => box.Value; return read(); } }',
                               'State.Drops = 0; int value = Helpers.First() + Helpers.Second(); '
                               'return value == 21 && State.Drops == 2 ? 42 : 1;', True),
        'multiple_lambdas': ('namespace Helpers { public int Work() { raii Box a = new Box(10); '
                             'raii Box b = new Box(11); raii Read one = [move a] () => a.Value; '
                             'raii Read two = [move b] () => b.Value; return one() + two(); } }',
                             'State.Drops = 0; int value = Helpers.Work(); '
                             'return value == 21 && State.Drops == 2 ? 42 : 1;', True),
        'noncapturing': ('namespace Helpers { public int Work() { raii Read read = () => 21; '
                         'return read() + read(); } }', 'return Helpers.Work();', True),
        'class_move': ('class Helper { public static int Work() { raii Box box = new Box(21); '
                       'raii Read read = [move box] () => box.Value; return read(); } }',
                       'State.Drops = 0; int value = Helper.Work(); return value == 21 && State.Drops == 1 ? 42 : 1;', True),
        'move_twice': ('namespace Helpers { public int Work() { raii Box box = new Box(21); '
                       'raii Read read = [move box] () => box.Value; return read() + read(); } }',
                       'return Helpers.Work();', False),
    }
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-namespace-lambda-') as temp:
        root = Path(temp)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>')
        for name, (declarations, body, valid) in cases.items():
            (root / 'Main.ns').write_text(head + declarations + ' class Program { public static int Main() { ' + body + ' } }')
            checked = run([compiler, '--check-project-method-bodies', project], root)
            if (checked.returncode == 0) != valid:
                failures.append((name, 'analysis', checked.returncode))
                continue
            if not valid:
                if 'FnOnce' not in checked.stdout + checked.stderr:
                    failures.append((name, 'diagnostic'))
                continue
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                built = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                             '-o', executable, '--no-artifact-cache', '--no-error-log'], root)
                if built.returncode or run([executable], root).returncode != 42:
                    failures.append((name, level))
            if name in ('copy', 'ref', 'move', 'move_parameter', 'multiple_functions',
                        'multiple_lambdas', 'noncapturing', 'class_move'):
                for level in ('O0', 'O2'):
                    executable = root / (name + '-' + level + '-debug.exe')
                    built = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-g',
                                 '-o', executable, '--no-artifact-cache', '--no-error-log'], root)
                    if built.returncode or run([executable], root).returncode != 42:
                        failures.append((name, level + '-debug'))
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
