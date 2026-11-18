"""LKF-061: overloads that differ only in `ref`/`out`/`in` are selected by the keyword written.

Historical builds reported "ambiguous instance method call" for `F(ref a)`, `F(out o)` and `F(in a)`
when `F(ref int)`, `F(out int)`, `F(in int)` and `F(int)` coexisted: the three keywords share the
pointer representation, so every keyword overload was viable with the same conversion. The valid
program calls each overload of a static and an instance method set and checks the side effects; it
runs at O0 and O2. Each invalid call writes a keyword that no overload declares, or none where only
keyword overloads exist, and must still be rejected with a precise diagnostic.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

VALID = '''class S
{
    public static int F(ref int x) { x = x + 1; return 1; }
    public static int F(out int x) { x = 7; return 2; }
    public static int F(in int x) { return 3 + x - x; }
    public static int F(int x) { return 4; }
}
class C
{
    public int G(ref int x) { x = x * 2; return 10; }
    public int G(out int x) { x = 9; return 20; }
    public int G(in int x) { return 30 + x - x; }
}
int Main()
{
    int a = 5;
    int r = S.F(ref a);
    int o;
    int q = S.F(out o);
    int i = S.F(in a);
    int v = S.F(a);
    if (r != 1 || a != 6) return 1;
    if (q != 2 || o != 7) return 2;
    if (i != 3) return 3;
    if (v != 4) return 4;
    C c = new C();
    int p;
    if (c.G(ref a) != 10 || a != 12) return 5;
    if (c.G(out p) != 20 || p != 9) return 6;
    if (c.G(in a) != 30 || a != 12) return 7;
    return 42;
}
'''

INVALID = {
    'sole overload, wrong keyword': (
        'class S { public static int F(ref int x) { return x; } }\n'
        'int Main() { int a = 1; return S.F(in a); }\n',
        "argument 1 of 'F' must be passed with 'ref'"),
    'sole overload, missing keyword': (
        'class S { public static int F(out int x) { x = 1; return x; } }\n'
        'int Main() { int a = 1; return S.F(a); }\n',
        "argument 1 of 'F' must be passed with 'out'"),
    'keyword no overload declares': (
        'class S { public static int F(ref int x) { return x; } public static int F(out int x) { x = 1; return x; } }\n'
        'int Main() { int a = 1; return S.F(in a); }\n',
        "no viable overload of 'F' accepts the argument types"),
    'no keyword, only keyword overloads': (
        'class S { public static int F(ref int x) { return x; } public static int F(in int x) { return x; } }\n'
        'int Main() { int a = 1; return S.F(a); }\n',
        "no viable overload of 'F' accepts the argument types"),
}

PROJECT = ('<Project><PropertyGroup><Name>PassKind</Name><OutputType>Exe</OutputType>'
           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" /></ItemGroup></Project>')


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=120)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-pass-kind-overloads-') as temporary:
        root = Path(temporary)

        def write(folder, source):
            folder.mkdir()
            (folder / 'Program.ns').write_text(source, encoding='utf-8')
            (folder / 'Case.nsproj').write_text(PROJECT, encoding='utf-8')
            return folder / 'Case.nsproj'

        for level in ('O0', 'O2'):
            folder = root / ('valid-' + level)
            case = write(folder, VALID)
            build = run([compiler, '-' + level, '--emit-project-oop-exe', str(case), '-o',
                         str(folder / 'case.exe'), '--no-artifact-cache', '--no-error-log'], folder)
            if build.returncode:
                failures.append((level, 'build', build.stdout + build.stderr))
                continue
            executed = run([str(folder / 'case.exe')], folder)
            print(level, 'exit', executed.returncode, flush=True)
            if executed.returncode != 42:
                failures.append((level, 'run', executed.returncode))
        for index, (name, (source, expected)) in enumerate(INVALID.items()):
            case = write(root / ('invalid-' + str(index)), source)
            result = run([compiler, '--check-project-method-bodies', str(case)], case.parent)
            output = result.stdout + result.stderr
            print(name, 'exit', result.returncode, output.strip().splitlines()[-1:], flush=True)
            if result.returncode == 0 or expected not in output or 'ambiguous' in output:
                failures.append((name, result.returncode, output))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('pass_kind_overloads: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
