"""FnOnce locals used as alternatives of a directly invoked conditional."""
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
    prefix = ('delegate int Read(); class Box { public int Value; public Box(int value) '
              '{ Value = value; } } class Program { public static int Main() { '
              'raii Box box = new Box(21); Read action = [move box] () => box.Value; ')
    other = 'raii Box b = new Box(21); Read next = [move b] () => b.Value; '
    cases = {
        'prior_then_conditional': ('int n = 0; int x = action(); return x + (n == 0 ? action : action)();', False, None),
        'conditional_then_local': ('int n = 0; int x = (n == 0 ? action : action)(); return x + action();', False, None),
        'same_conditional_twice': ('int n = 0; int x = (n == 0 ? action : action)(); return x + (n == 0 ? action : action)();', False, None),
        'alias_alternative': ('Read alias = action; int n = 0; int x = action(); return x + (n == 0 ? alias : action)();', False, None),
        'two_alternatives_uncovered': (other + 'int n = 1; int x = (n == 0 ? action : next)(); return x + next();', True, None),
        'nested_uncovered': (other + 'int n = 0; int x = action(); return x + (n == 0 ? (n == 1 ? next : action) : next)();', True, None),
        'nested_same_environment': ('Read alias = action; int n = 0; int x = action(); '
                                    'return x + (n == 0 ? (n == 1 ? alias : action) : alias)();', False, None),
        'loop': ('int n = 0; while (n < 2) { (n == 0 ? action : action)(); n = n + 1; } return n;', False, None),
        'one_conditional': (other + 'int n = 0; int x = (n == 0 ? action : next)(); '
                            'delete action; delete next; return x * 2;', True, 42),
        'literal_true': (other + 'int x = (true ? action : next)(); int y = next(); '
                         'delete action; delete next; return x + y;', True, 42),
        'literal_false': (other + 'int x = (false ? next : action)(); int y = next(); '
                          'delete action; delete next; return x + y;', True, 42),
        'constant_bool': (other + 'const bool select = true; int x = (select ? action : next)(); int y = next(); '
                          'delete action; delete next; return x + y;', True, 42),
        'known_variable': (other + 'int n = 0; int x = (n == 0 ? action : next)(); int y = next(); '
                           'delete action; delete next; return x + y;', True, 42),
        'exclusive_branches': ('Read alias = action; int n = 0; int x = 0; '
                               'if (n == 0) { x = action(); } else { x = alias(); } '
                               'delete action; return x * 2;', True, 42),
        'fn_twice': ('int n = 21; Read plain = [copy n] () => n; int x = (n == 21 ? plain : plain)(); '
                     'int y = (n == 21 ? plain : plain)(); delete plain; delete action; return x + y;', True, 42),
    }
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-fnonce-conditional-') as temp:
        root = Path(temp)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>')
        for name, (body, valid, expected) in cases.items():
            (root / 'Main.ns').write_text(prefix + body + ' } }')
            checked = run([compiler, '--check-project-method-bodies', project], root)
            if (checked.returncode == 0) != valid:
                failures.append((name, 'analysis', checked.returncode))
                continue
            if not valid:
                if 'FnOnce' not in checked.stdout + checked.stderr:
                    failures.append((name, 'diagnostic'))
                continue  # Do not execute potential double invocations.
            if expected is None:
                continue  # Known coverage limit; do not execute.
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
