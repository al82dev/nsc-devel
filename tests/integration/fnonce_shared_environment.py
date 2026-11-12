"""FnOnce consumption follows local aliases of a closure environment."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(args, root):
    result = subprocess.run([str(x) for x in args], cwd=root, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline([str(x) for x in args]),
          'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    cases = {
        'direct': ('int x = action(); return x + action();', False, None),
        'alias_forward': ('Read alias = action; int x = action(); return x + alias();', False, None),
        'alias_reverse': ('Read alias = action; int x = alias(); return x + action();', False, None),
        'alias_chain': ('Read a = action; Read b = a; int x = action(); return x + b();', False, None),
        'late_alias': ('int x = action(); Read a = action; return x + a();', False, None),
        'branch': ('Read a = action; int c = 0; if (c == 0) { action(); } return a();', False, None),
        'loop_alias': ('Read a = action; int c = 0; while (c < 2) { a(); c = c + 1; } return 0;', False, None),
        'for_alias': ('Read a = action; for (int c = 0; c < 2; c = c + 1) { a(); } return 0;', False, None),
        'loop_inner_alias': ('Read old = action; int c = 0; while (c < 2) { '
            'Read inner = old; inner(); c = c + 1; } return 0;', False, None),
        'loop_inner_reassigned': ('Read old = action; int c = 0; while (c < 2) { '
            'Read inner = old; inner(); raii Box b = new Box(1); '
            'inner = [move b] () => b.Value; c = c + 1; } return 0;', False, None),
        'loop_branch_alias': ('Read old = action; int c = 0; while (c < 2) { '
            'Read inner = action; if (c == 0) { inner = old; } inner(); c = c + 1; } return 0;', False, None),
        'loop_chain_alias': ('Read old = action; Read outer = old; int c = 0; while (c < 2) { '
            'Read inner = outer; inner(); c = c + 1; } return 0;', False, None),
        'loop_break': ('Read a = action; int value = 0; while (true) { value = a(); break; } '
            'delete action; return value;', True, 42),
        'loop_fresh_each': ('int c = 0; int value = 0; while (c < 2) { '
            'raii Box b = new Box(21); Read fresh = [move b] () => b.Value; '
            'value = value + fresh(); delete fresh; c = c + 1; } '
            'delete action; return value;', True, 42),
        'exclusive_branches': ('Read a = action; int c = 0; int value = 0; '
            'if (c == 0) { value = action(); } else { value = a(); } '
            'delete action; return value;', True, 42),
        'possible_alias_after_branch': ('Read a = action; raii Box b = new Box(1); '
            'Read next = [move b] () => b.Value; int c = 0; if (c == 0) { a = next; } '
            'int x = action(); return x + a();', False, None),
        'reassigned_alias': ('Read a = action; raii Box b = new Box(1); '
            'Read next = [move b] () => b.Value; a = next; int x = action(); '
            'int y = a(); delete action; delete next; return x + y;', True, 43),
        'old_alias_after_reassign': ('Read a = action; Read prior = action; raii Box b = new Box(1); '
            'Read next = [move b] () => b.Value; action = next; int x = a(); '
            'return x + prior();', False, None),
        'old_alias_survives_loop': ('Read old = action; int c = 0; while (c < 2) { '
            'int value = action(); raii Box fresh = new Box(21); '
            'action = [move fresh] () => fresh.Value; c = c + 1; } return 0;', True, None),
        'single_alias': ('Read a = action; int x = a(); delete action; return x;', True, 42),
        'independent': ('raii Box b = new Box(1); Read next = [move b] () => b.Value; '
            'int x = action(); int y = next(); delete action; delete next; return x + y;', True, 43),
    }
    prefix = ('delegate int Read(); class Box { public int Value; public Box(int value) '
              '{ Value = value; } } class Program { public static int Main() { '
              'raii Box box = new Box(42); Read action = [move box] () => box.Value; ')
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-fnonce-alias-') as temp:
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
                continue  # Never execute double invocation.
            if expected is None:
                continue  # Analysis-only control; ownership cleanup is outside this probe.
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
