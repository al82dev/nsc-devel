"""Direct calls of FnOnce parameters through source-defined static methods."""
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
    prefix = ('delegate int Read(); class Box { public int Value; '
              'public Box(int value) { Value = value; } } class Program { ')
    capture = 'raii Box box = new Box(21); Read action = [move box] () => box.Value; '
    cases = {
        'callee_twice': ('static int Invoke(Read a) { int x = a(); return x + a(); }',
                         capture + 'return Invoke(action);', False, None),
        'method_twice': ('static int Invoke(Read a) { return a(); }',
                         capture + 'int x = Invoke(action); return x + Invoke(action);', False, None),
        'method_then_local': ('static int Invoke(Read a) { return a(); }',
                              capture + 'int x = Invoke(action); return x + action();', False, None),
        'local_then_method': ('static int Invoke(Read a) { return a(); }',
                              capture + 'int x = action(); return x + Invoke(action);', False, None),
        'aliases_two_parameters': ('static int Invoke(Read a, Read b) { return a() + b(); }',
                                   capture + 'Read other = action; return Invoke(action, other);', False, None),
        'loop': ('static int Invoke(Read a) { return a(); }',
                 capture + 'int n = 0; while (n < 2) { Invoke(action); n = n + 1; } return n;', False, None),
        'fn_once_single': ('static int Invoke(Read a) { return a(); }',
                           capture + 'int x = Invoke(action); delete action; return x * 2;', True, 42),
        'fn_once_exclusive_branches': ('static int Invoke(Read a) { int n = 1; '
                                       'if (n == 1) { return a(); } else { return a(); } }',
                                       capture + 'int x = Invoke(action); delete action; return x * 2;', True, 42),
        'unused_parameter': ('static int Ignore(Read a) { return 0; }',
                             capture + 'Ignore(action); int x = action(); delete action; return x * 2;', True, 42),
        'independent': ('static int Invoke(Read a) { return a(); }', capture +
                        'raii Box b = new Box(21); Read second = [move b] () => b.Value; '
                        'int x = Invoke(action); int y = Invoke(second); delete action; delete second; return x + y;', True, 42),
        'fn_multiple': ('static int Invoke(Read a) { int x = a(); return x + a(); }',
                        'int value = 21; Read action = [copy value] () => value; '
                        'int x = Invoke(action); delete action; return x;', True, 42),
    }
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-fnonce-parameter-') as temp:
        root = Path(temp)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>')
        for name, (members, body, valid, expected) in cases.items():
            (root / 'Main.ns').write_text(prefix + members + ' public static int Main() { ' + body + ' } }')
            checked = run([compiler, '--check-project-method-bodies', project], root)
            if (checked.returncode == 0) != valid:
                failures.append((name, 'analysis', checked.returncode))
                continue
            if not valid:
                if 'FnOnce' not in checked.stdout + checked.stderr:
                    failures.append((name, 'diagnostic'))
                continue  # Do not execute a double invocation.
            for level in ('O0', 'O2'):
                exe = root / (name + '-' + level + '.exe')
                built = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                             '-o', exe, '--no-artifact-cache', '--no-error-log'], root)
                if built.returncode or run([exe], root).returncode != expected:
                    failures.append((name, level))
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
