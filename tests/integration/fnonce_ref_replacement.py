"""Known simple ref/out replacement of a local FnOnce delegate."""
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
    head = ('delegate int Read(); class Box { public int Value; public Box(int value) { Value = value; } } '
            'class Program { static void Replace(ref Read x, Read y) { x = y; } '
            'static void Set(out Read x, Read y) { x = y; } static void Keep(ref Read x) { } '
            'public static int Main() { raii Box a = new Box(21); Read one = [move a] () => a.Value; '
            'Read old = one; raii Box b = new Box(21); Read fresh = [move b] () => b.Value; ')
    cases = {
        'ref_replaced': ('int x = one(); Replace(ref one, fresh); int y = one(); delete old; delete fresh; return x + y;', True),
        'out_replaced': ('int x = one(); Set(out one, fresh); int y = one(); delete old; delete fresh; return x + y;', True),
        'named_replaced': ('int x = one(); Replace(y: fresh, x: ref one); int y = one(); delete old; delete fresh; return x + y;', True),
        'both_branches_replace': ('int x = one(); int n = 0; if (n == 0) { Replace(ref one, fresh); } '
                                  'else { Replace(ref one, fresh); } int y = one(); delete old; delete fresh; return x + y;', True),
        'ref_before': ('Replace(ref one, fresh); int x = one(); delete old; delete fresh; return x * 2;', True),
        'ref_kept': ('int x = one(); Keep(ref one); return x + one();', False),
        'ref_same': ('int x = one(); Replace(ref one, old); return x + one();', False),
        'ref_self': ('int x = one(); Replace(ref one, one); return x + one();', False),
        'old_alias_consumed': ('int x = one(); Replace(ref one, fresh); return x + old();', False),
        'replacement_already_consumed': ('int x = fresh(); Replace(ref one, fresh); return x + one();', False),
        'one_branch_keeps': ('int x = one(); int n = 0; if (n == 0) { Replace(ref one, fresh); } '
                             'else { Keep(ref one); } return x + one();', False),
    }
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-fnonce-ref-replace-') as temp:
        root = Path(temp)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>')
        for name, (body, valid) in cases.items():
            (root / 'Main.ns').write_text(head + body + ' } }')
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
