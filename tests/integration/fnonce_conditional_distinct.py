"""Conditional invocation with distinct known closure environments."""
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
              'raii Box a = new Box(21); Read one = [move a] () => a.Value; '
              'raii Box b = new Box(21); Read two = [move b] () => b.Value; ')
    cases = {
        'const_both_consumed': ('int n = 0; int x = one(); int y = two(); '
                          'return x + y + (n == 0 ? one : two)();', False, None),
        'const_both_consumed_alias': ('const Read x = one; const Read y = two; int n = 0; int p = one(); int q = two(); '
                                'return p + q + (n == 0 ? x : y)();', False, None),
        'const_both_consumed_branches': ('int n = 0; if (n == 0) { one(); two(); } else { one(); two(); } '
                                   'return (n == 0 ? one : two)();', False, None),
        'correlated_valid': ('int n = 0; int x = 0; if (n == 0) { x = one(); } else { x = two(); } '
                             'int y = (n == 0 ? two : one)(); delete one; delete two; return x + y;', True, 42),
        'const_correlated_valid': ('int n = 0; int x = 0; if (n == 0) { x = one(); } else { x = two(); } '
                                   'int y = (n == 0 ? two : one)(); delete one; delete two; return x + y;', True, 42),
        'one_then_other': ('int n = 0; int x = (n == 0 ? one : two)(); int y = two(); '
                           'delete one; delete two; return x + y;', True, 42),
        'single': ('int n = 0; int x = (n == 0 ? one : two)(); delete one; delete two; return x * 2;', True, 42),
        'reassigned_valid': ('Read old = one; int x = one(); raii Box c = new Box(21); '
                             'Read fresh = [move c] () => c.Value; one = fresh; int n = 0; '
                             'int y = (n == 0 ? one : two)(); delete old; delete fresh; delete two; return x + y;', True, 42),
        'ref_reassigned_valid': ('Read old = one; int x = one(); int y = two(); raii Box c = new Box(21); '
                                 'Read fresh = [move c] () => c.Value; Replace(ref one, fresh); int n = 0; '
                                 'int z = (n == 0 ? one : two)(); delete old; delete fresh; delete two; '
                                 'return x + y + z - 21;', True, 42),
        'out_reassigned_valid': ('Read old = one; int x = one(); int y = two(); raii Box c = new Box(21); '
                                 'Read fresh = [move c] () => c.Value; ReplaceOut(out one, fresh); int n = 0; '
                                 'int z = (n == 0 ? one : two)(); delete old; delete fresh; delete two; '
                                 'return x + y + z - 21;', True, 42),
        'one_branch_consumed_uncovered': ('int n = 0; int x = one(); return x + (n == 0 ? one : two)();', True, None),
        'mutable_both_uncovered': ('int n = 0; int x = one(); int y = two(); '
                                   'return x + y + (n == 0 ? one : two)();', True, None),
    }
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-fnonce-distinct-') as temp:
        root = Path(temp)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>')
        for name, (body, valid, expected) in cases.items():
            setup = (prefix.replace('Read one =', 'const Read one =').replace('Read two =', 'const Read two =')
                     if name.startswith('const_') else prefix)
            (root / 'Main.ns').write_text(setup + body + ' } static void Replace(ref Read a, Read b) { a = b; } '
                                         'static void ReplaceOut(out Read a, Read b) { a = b; } }')
            checked = run([compiler, '--check-project-method-bodies', project], root)
            if (checked.returncode == 0) != valid:
                failures.append((name, 'analysis', checked.returncode))
                continue
            if not valid:
                if 'FnOnce' not in checked.stdout + checked.stderr:
                    failures.append((name, 'diagnostic'))
                continue
            if expected is None:
                continue  # Known uncovered case; do not execute.
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
