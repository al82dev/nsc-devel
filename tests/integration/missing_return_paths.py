"""LKF-058: a method with a result is rejected when a reachable path ends without `return`.

Historical builds accepted such a path and its call had undefined behaviour (0x80000003 at O0,
1 at O2). The valid program returns on both branches and has an endless loop with no end to
reach; it runs at O0 and O2. Each invalid method leaves one path without a value: an `if` without
`else`, and a loop that can finish without returning.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

VALID = '''class S
{
    public static int Both(bool c) { if (c) { return 1; } else { return 2; } }
    public static int Endless(int n) { while (true) { if (n > 2) { return n; } n = n + 1; } }
}
int Main() { return S.Both(false) * 10 + S.Endless(0); }
'''

INVALID = {
    'if without else': ('class S { public static int F(bool c) { if (c) { return 1; } } }\nint Main() { return 0; }\n',
                        "not all reachable control paths return a value from function 'F'"),
    'loop that can finish': ('class S { public static int G(int n) {\n while (n > 0) { if (n == 3) { return n; } '
                             'n = n - 1; } } }\nint Main() { return 0; }\n',
                             "not all reachable control paths return a value from function 'G'"),
}

PROJECT = ('<Project><PropertyGroup><Name>MissingReturn</Name><OutputType>Exe</OutputType>'
           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" /></ItemGroup></Project>')


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=120)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-missing-return-') as temporary:
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
            if executed.returncode != 23:
                failures.append((level, 'run', executed.returncode))
        for index, (name, (source, expected)) in enumerate(INVALID.items()):
            case = write(root / ('invalid-' + str(index)), source)
            result = run([compiler, '--check-project-method-bodies', str(case)], case.parent)
            output = result.stdout + result.stderr
            print(name, 'exit', result.returncode, flush=True)
            if result.returncode == 0 or expected not in output:
                failures.append((name, result.returncode, output))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('missing_return_paths: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
