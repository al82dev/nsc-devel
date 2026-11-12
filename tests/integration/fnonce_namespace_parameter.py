"""Direct FnOnce parameter invocation through selected namespace functions."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, root):
    command = [str(x) for x in command]
    result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                            errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    head = ('delegate int Read(); class Box { public int Value; public Box(int value) { Value = value; } } ')
    capture = 'raii Box box = new Box(21); Read action = [move box] () => box.Value; '
    cases = {
        'callee_twice': ('namespace Helpers { public int Invoke(Read a) { int x = a(); return x + a(); } }',
                         capture + 'return Helpers.Invoke(action);', False),
        'namespace_twice': ('namespace Helpers { public int Invoke(Read a) { return a(); } }',
                            capture + 'int x = Helpers.Invoke(action); return x + Helpers.Invoke(action);', False),
        'namespace_then_local': ('namespace Helpers { public int Invoke(Read a) { return a(); } }',
                                 capture + 'int x = Helpers.Invoke(action); return x + action();', False),
        'local_then_namespace': ('namespace Helpers { public int Invoke(Read a) { return a(); } }',
                                 capture + 'int x = action(); return x + Helpers.Invoke(action);', False),
        'named_twice': ('namespace Helpers { public int Invoke(Read a) { return a(); } }',
                        capture + 'int x = Helpers.Invoke(a: action); return x + Helpers.Invoke(a: action);', False),
        'same_environment_two_parameters': ('namespace Helpers { public int Invoke(Read a, Read b) { return a() + b(); } }',
                                            capture + 'Read alias = action; return Helpers.Invoke(action, alias);', False),
        'single': ('namespace Helpers { public int Invoke(Read a) { return a(); } }',
                   capture + 'int x = Helpers.Invoke(action); delete action; return x * 2;', True),
        'ignore': ('namespace Helpers { public int Ignore(Read a) { return 0; } }',
                   capture + 'Helpers.Ignore(action); int x = action(); delete action; return x * 2;', True),
        'fn_twice': ('namespace Helpers { public int Invoke(Read a) { int x = a(); return x + a(); } }',
                     'int n = 21; Read action = [copy n] () => n; int x = Helpers.Invoke(action); delete action; return x;', True),
        'overload_selected': ('namespace Helpers { public int Invoke(Read a) { return a(); } '
                              'public int Invoke(int a) { return a; } }',
                              capture + 'int x = Helpers.Invoke(action); delete action; return x * 2;', True),
        'namespace_selected': ('namespace Helpers { public int Invoke(Read a) { return 0; } } '
                               'namespace Other { public int Invoke(Read a) { return a() + a(); } }',
                               capture + 'Helpers.Invoke(action); int x = action(); delete action; return x * 2;', True),
        'namespace_caller_twice': ('namespace Helpers { public int Invoke(Read a) { return a(); } '
                                   'public int Entry() { ' + capture + 'int x = Invoke(action); return x + action(); } }',
                                   'return Helpers.Entry();', False),
        'namespace_caller_single': ('namespace Helpers { public int Invoke(Read a) { return a(); } '
                                    'public int Entry() { ' + capture + 'int x = Invoke(action); delete action; return x * 2; } }',
                                    'return Helpers.Entry();', True),
    }
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-fnonce-namespace-') as temp:
        root = Path(temp)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>')
        for name, (members, body, valid) in cases.items():
            (root / 'Main.ns').write_text(head + members + ' class Program { public static int Main() { ' + body + ' } }')
            checked = run([compiler, '--check-project-method-bodies', project], root)
            if (checked.returncode == 0) != valid:
                failures.append((name, 'analysis', checked.returncode))
                continue
            if not valid:
                if 'FnOnce' not in checked.stdout + checked.stderr:
                    failures.append((name, 'diagnostic'))
                continue
            for level in ('O0', 'O2'):
                exe = root / (name + '-' + level + '.exe')
                built = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                             '-o', exe, '--no-artifact-cache', '--no-error-log'], root)
                if built.returncode or run([exe], root).returncode != 42:
                    failures.append((name, level))
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
