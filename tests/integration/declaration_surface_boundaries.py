"""Declaration forms at the edge of the supported surface (LKF-505, 591, 595, 604).

Each program either exits 42 at O0 and O2 or is rejected with the exact diagnostic, and every
rejection has an accepted control. Two defects were fixed with these cases: a zero-length fixed-size
array local (`int[0] a;`) reached LLVM lowering as an unmappable type instead of a diagnostic
(LKF-505), and a comma-separated field list produced a cascade of parse errors instead of saying that
each field needs its own declaration (LKF-604).
"""
from pathlib import Path
import subprocess
import sys
import tempfile

MAIN = 'class Program { public static int Main() { BODY } }'

CASES = {
    'zero-length fixed-size array local': (MAIN.replace('BODY', 'int[0] a; return 42;'),
        'a fixed-size array local must have a positive length'),
    'fixed-size array local': (MAIN.replace('BODY', 'int[3] a; a[2] = 42; return a[2];'), 42),
    'zero-length fixed-size array field': ('struct S { public int[0] Items; }\n' + MAIN.replace('BODY', 'return 42;'),
        'class/struct field array count must be a positive compile-time integer expression'),
    'large fixed-size array field': ('struct S { public byte[4096] Items; }\n' +
        MAIN.replace('BODY', 'S s; s.Items[4095] = 42; return s.Items[4095];'), 42),
    'comma-separated instance fields': ('class P { public int A, B; }\n' + MAIN.replace('BODY', 'return 42;'),
        'a field declaration declares one field; write a separate declaration for each name'),
    'comma-separated static fields': ('class P { public static int A = 40, B = 2; }\n' +
        MAIN.replace('BODY', 'return 42;'),
        'a field declaration declares one field'),
    'one declaration per field': ('class P { public int A; public int B; }\n' +
        MAIN.replace('BODY', 'P p = new P(); p.A = 40; p.B = 2; return p.A + p.B;'), 42),
    'comma-separated locals': (MAIN.replace('BODY', 'int a = 40, b = 2; return a + b;'), 42),
    'reference local with an inferred type': (MAIN.replace('BODY',
        'int[] a = new int[2]; ref var x = ref a[0]; x = 42; return a[0];'),
        'reference locals require an explicit element type'),
    'reference local with an explicit type': (MAIN.replace('BODY',
        'int[] a = new int[2]; ref int x = ref a[0]; x = 42; return a[0];'), 42),
    'setter-only property': ('class P { private int v; public int V { set { v = value; } } }\n' +
        MAIN.replace('BODY', 'return 42;'), 'property without one cannot be bound'),
    'property with a getter and a setter': ('class P { private int v; public int V { get { return v; } '
        'set { v = value; } } }\n' + MAIN.replace('BODY', 'P p = new P(); p.V = 42; return p.V;'), 42),
    'nested type declaration': ('class Outer { public class Inner { public int V = 42; } }\n' +
        MAIN.replace('BODY', 'Outer.Inner i = new Outer.Inner(); return i.V;'), 42),
}

PROJECT = ('<Project><PropertyGroup><Name>P</Name><OutputType>Exe</OutputType>'
           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" /></ItemGroup></Project>')


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=120)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-declaration-surface-') as temporary:
        root = Path(temporary)
        for index, (name, (source, expected)) in enumerate(CASES.items()):
            folder = root / ('case-' + str(index))
            folder.mkdir()
            (folder / 'Program.ns').write_text(source, encoding='utf-8')
            project = folder / 'Case.nsproj'
            project.write_text(PROJECT, encoding='utf-8')
            if isinstance(expected, str):
                result = run([compiler, '--emit-project-oop-exe', str(project), '-o', str(folder / 'rejected.exe'),
                              '--no-artifact-cache', '--no-error-log'], folder)
                output = result.stdout + result.stderr
                print(name, 'exit', result.returncode, flush=True)
                if result.returncode == 0 or expected not in output or 'internal error' in output or \
                        'lowering failed' in output:
                    failures.append((name, 'rejection', output.strip()[-300:]))
                continue
            for level in ('O0', 'O2'):
                exe = folder / ('case-' + level + '.exe')
                build = run([compiler, '-' + level, '--emit-project-oop-exe', str(project), '-o', str(exe),
                             '--no-artifact-cache', '--no-error-log'], folder)
                if build.returncode:
                    failures.append((name, level, 'build', (build.stdout + build.stderr).strip()[-300:]))
                    continue
                executed = run([str(exe)], folder)
                print(name, level, 'exit', executed.returncode, flush=True)
                if executed.returncode != expected:
                    failures.append((name, level, 'exit', executed.returncode, 'expected', expected))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('declaration_surface_boundaries: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
