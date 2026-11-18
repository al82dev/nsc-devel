"""Check owner provenance of views constructed directly inside tuples."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


REPO = Path(__file__).resolve().parents[2]
TYPES = '''public borrowed struct View<lifetime a, T> {
 private borrowed[a] T* target;
 public View(ref[a] T value) { unsafe("view") { target = &value; } }
 public ref[this] T Get() { unsafe("view") { return ref *target; } }
}
public struct Owner { public int Value; ~Owner() { } }
'''
OWNER = 'Owner owner; owner.Value = 42; '
PAIR = '(View<int>, int) pair = (new View<int>(ref owner.Value), 0); '
EXTRACT = 'var (view, n) = pair; '
CASES = {
    'live': ('class Program { public static int Main() { ' + OWNER + PAIR + EXTRACT +
             'return view.Get(); } }', None),
    'escape': ('class Program { public static int Main() { View<int> result = default(View<int>); { ' +
               OWNER + PAIR + EXTRACT + 'result = view; } return result.Get(); } }',
               'cannot be stored in a longer-lived local'),
    'replace': ('class Program { public static int Main() { ' + OWNER + PAIR + EXTRACT +
                'owner = default(Owner); return view.Get(); } }', 'while a borrowed local is alive'),
    'parameter_live': ('class Program { static int Read(ref Owner owner) { ' + PAIR + EXTRACT +
                       'return view.Get(); } public static int Main() { ' + OWNER +
                       'return Read(ref owner); } }', None),
    'global_live': ('class State { public static int Value; } class Program { public static int Main() { '
                    'State.Value = 42; (View<int>, int) pair = (new View<int>(ref State.Value), 0); '
                    + EXTRACT + 'return view.Get(); } }', None),
    'branch_live': ('class Program { public static int Main() { ' + OWNER +
                    'bool choose = true; (View<int>, int) pair = choose ? '
                    '(new View<int>(ref owner.Value), 0) : (new View<int>(ref owner.Value), 1); '
                    + EXTRACT + 'return view.Get(); } }', None),
    'parameter_scope': ('class Program { static int Read(ref Owner owner) { '
                        'View<int> result = default(View<int>); { ' + PAIR + EXTRACT +
                        'result = view; } return result.Get(); } public static int Main() { ' + OWNER +
                        'return Read(ref owner); } }', None),
    'global_scope': ('class State { public static int Value; } class Program { public static int Main() { '
                     'State.Value = 42; View<int> result = default(View<int>); { '
                     '(View<int>, int) pair = (new View<int>(ref State.Value), 0); ' + EXTRACT +
                     'result = view; } return result.Get(); } }', None),
    'multiple_live': ('class Program { public static int Main() { Owner first; first.Value = 42; '
                      'Owner second; second.Value = 42; bool choose = false; '
                      '(View<int>, int) pair = choose ? (new View<int>(ref first.Value), 0) : '
                      '(new View<int>(ref second.Value), 1); ' + EXTRACT + 'return view.Get(); } }', None),
    'multiple_escape': ('class Program { public static int Main() { Owner first; first.Value = 42; '
                        'View<int> result = default(View<int>); { Owner second; second.Value = 42; '
                        'bool choose = false; (View<int>, int) pair = choose ? '
                        '(new View<int>(ref first.Value), 0) : (new View<int>(ref second.Value), 1); '
                        + EXTRACT + 'result = view; } return result.Get(); } }',
                        'cannot be stored in a longer-lived local'),
}


def invoke(command):
    command = [str(value) for value in command]
    result = subprocess.run(command, cwd=REPO, capture_output=True, text=True,
                            errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    temporary = Path(os.environ.get('NSC_TEST_TEMP_ROOT') or os.environ['TEMP']).resolve()
    if temporary.drive.upper() != 'A:' or not temporary.is_dir():
        raise ValueError('temporary root must be an existing directory on A:')
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-inline-tuple-view-', dir=temporary) as directory:
        root = Path(directory)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>InlineTupleView</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
        for name, (body, expected_error) in CASES.items():
            (root / 'Main.ns').write_text(TYPES + body, encoding='utf-8')
            result = invoke([compiler, '--check-project-method-bodies', project])
            output = result.stdout + result.stderr
            if expected_error is not None:
                if result.returncode == 0 or expected_error not in output:
                    failures.append((name, 'negative-not-rejected'))
                continue
            if result.returncode != 0:
                failures.append((name, 'positive-check'))
                continue
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                built = invoke([compiler, '--emit-project-oop-exe', project, '-' + level,
                                '-o', executable, '--no-artifact-cache'])
                if built.returncode != 0:
                    failures.append((name, level, 'build'))
                elif invoke([executable]).returncode != 42:
                    failures.append((name, level, 'run'))
    print('FAILURES', failures, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
