"""Dispatch, lowering and storage at the edge of the supported surface (LKF-332..LKF-480).

Each program either exits 42 at O0 and O2 or is rejected with the exact diagnostic, and every
rejection has an accepted control. Defects fixed with these cases:

- An interface method found a member of the same signature that could not implement it (private
  or static) and stopped, so the public implementation in a base class was never used (LKF-391).
- A namespace function definition kept the parameter names of its prototype, so a renamed parameter
  was an unknown identifier in the body (LKF-392).
- The governing value of a match was allocated where the match was, so a match in a long loop
  overflowed the stack (LKF-442).
- A variable-length array declared in a loop body grew the stack on every iteration (LKF-332).
- A null-conditional assignment to an owned array element or owned field did not release the
  previous value (LKF-444).

The other cases pin verified boundaries with their controls.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

MAIN = 'class Program { public static int Main() { BODY } }'
CORE = 'using NativeSharp.Core;\n'
DROPS = 'class P { public static int Drops = 0; public int V = 1; ~P() { Drops = Drops + 1; } }\n'
I_B = 'interface I { int F(); }\nclass B { public int F() { return 42; } }\n'
CALL_I = MAIN.replace('BODY', 'I i = new D(); return i.F();')
HOLDER = 'class H { public owned P Item; }\n'
OWNED_REPLACE = 'a null-conditional assignment cannot replace an element or field that owns its value'
LOOP = 'int t = 0; int n = 64; '

CASES = {
    # LKF-391
    'private member over a public base implementation': (False, I_B + 'class D : B, I { private int F() '
        '{ return 1; } }\n' + CALL_I, 42),
    'static member over a public base implementation': (False, I_B + 'class D : B, I { public static int F() '
        '{ return 1; } }\n' + CALL_I, 42),
    'public base implementation': (False, I_B + 'class D : B, I { }\n' + CALL_I, 42),
    'more derived public implementation': (False, 'interface I { int F(); }\nclass B { public int F() '
        '{ return 1; } }\nclass D : B, I { public int F() { return 42; } }\n' + CALL_I, 42),
    'only a private member': (False, 'interface I { int F(); }\nclass D : I { private int F() { return 1; } }\n' +
        CALL_I, "does not implement interface method 'F()'"),
    # LKF-392
    'namespace prototype renamed in the definition': (False, 'namespace N { public int F(int x); '
        'public int F(int y) { return y; } }\n' + MAIN.replace('BODY', 'return N.F(42);'), 42),
    'namespace prototype then definition': (False, 'namespace N { public int F(int x); '
        'public int F(int x) { return x; } }\n' + MAIN.replace('BODY', 'return N.F(42);'), 42),
    # LKF-396 / LKF-397
    'opaque trait result': (False, 'trait IValue { int Get(); }\nstruct V : IValue { public int X; '
        'public int Get() { return X; } }\nclass G { public static some IValue Make() { V v; v.X = 42; '
        'return v; } }\n' + MAIN.replace('BODY', 'var v = G.Make(); return v.Get();'), 42),
    'opaque trait result with a homonym trait': (False, 'namespace A { public trait IValue { int Get(); } }\n'
        'namespace B { public trait IValue { int Other(); } }\nstruct V : A.IValue { public int X; '
        'public int Get() { return X; } }\nclass G { public static some A.IValue Make() { V v; v.X = 42; '
        'return v; } }\n' + MAIN.replace('BODY', 'var v = G.Make(); return v.Get();'), 42),
    # LKF-398
    'indexer called with ref arguments': (False, 'class G { public ref int this[ref int a, ref int b] '
        '{ get { return ref b; } } }\n' + MAIN.replace('BODY', 'G g = new G(); int x = 1; int y = 42; '
        'return g[ref x, ref y];'), "expected ']'"),
    # LKF-422
    'whole move after a field move': (False, 'class Item { public int V = 40; }\nstruct S { public owned Item A; '
        'public owned Item B; }\n' + MAIN.replace('BODY', 'S s; s.A = new Item(); s.B = new Item(); '
        'owned Item a = move(s.A); S t = move(s); return a.V + 2;'),
        "cannot move complete value 's' after one of its fields was moved"),
    # LKF-434 / LKF-435
    'long enum value': (False, 'enum E : long { Big = 5000000000 }\n' + MAIN.replace('BODY',
        'E e = E.Big; long v = (long)e; return v == 5000000000 ? 42 : 1;'), 42),
    'byte enum value above 127': (False, 'enum E : byte { Hi = 200 }\n' + MAIN.replace('BODY',
        'E e = E.Hi; int v = (int)e; return v == 200 ? 42 : 1;'), 42),
    'pointer minus zero': (False, MAIN.replace('BODY', 'int[2] a; unsafe("t") { int* p = &a[0]; int* q = p - 0; '
        'return q == p ? 42 : 1; }'), 42),
    # LKF-441
    'Dispose that declares throws': (True, CORE + 'class Res : IDisposable { public void Dispose() throws Exception '
        '{ throw new Exception(); } }\n' + MAIN.replace('BODY', 'return 42;'), 'outside interface contract'),
    # LKF-442
    'match on a scalar in a long loop': (False, MAIN.replace('BODY', 'int t = 0; for (int i = 0; i < 3000000; '
        'i = i + 1) { int k = i & 1; t = t + match (k) { 0 => 1, _ => 0 }; } return t == 1500000 ? 42 : 1;'), 42),
    'match on a struct in a long loop': (False, 'struct Q { public int A; public long B; public long C; }\n' +
        MAIN.replace('BODY', 'int t = 0; for (int i = 0; i < 3000000; i = i + 1) { Q q; q.A = i & 1; '
        't = t + match (q) { { A: 0 } => 1, _ => 0 }; } return t == 1500000 ? 42 : 1;'), 42),
    # LKF-332
    'variable-length array in a for loop': (False, MAIN.replace('BODY', LOOP + 'for (int i = 0; i < 200000; '
        'i = i + 1) { int[n] a; a[0] = i & 1; t = t + a[0]; } return t == 100000 ? 42 : 1;'), 42),
    'variable-length array in a while loop': (False, MAIN.replace('BODY', LOOP + 'int i = 0; while (i < 200000) '
        '{ int[n] a; a[n - 1] = 1; t = t + a[n - 1]; i = i + 1; } return t == 200000 ? 42 : 1;'), 42),
    'variable-length array in a do loop left by break': (False, MAIN.replace('BODY', LOOP + 'int i = 0; do '
        '{ int[n] a; a[0] = 1; t = t + a[0]; i = i + 1; if (i == 150000) { break; } } while (i < 200000); '
        'return t == 150000 ? 42 : 1;'), 42),
    'variable-length array in a loop expression': (False, MAIN.replace('BODY', LOOP + 'int i = 0; int r = loop '
        '{ int[n] a; a[0] = 1; t = t + a[0]; i = i + 1; if (i == 200000) { break t; } }; '
        'return r == 200000 ? 42 : 1;'), 42),
    'variable-length array in a foreach loop': (True, CORE + MAIN.replace('BODY', LOOP + 'int[] xs = '
        'new int[200000]; foreach (int x in xs) { int[n] a; a[0] = 1; t = t + a[0]; } delete[] xs; '
        'return t == 200000 ? 42 : 1;'), 42),
    'variable-length array before a loop survives it': (False, MAIN.replace('BODY', 'int n = 4; int[n] keep; '
        'keep[3] = 40; for (int i = 0; i < 100000; i = i + 1) { int[n] a; a[0] = i; } keep[3] = keep[3] + 2; '
        'return keep[3];'), 42),
    'stackalloc in a short loop': (False, MAIN.replace('BODY', 'int t = 0; for (int i = 0; i < 1000; i = i + 1) '
        '{ unsafe("t") { int* a = stackalloc int[64]; a[0] = i & 1; t = t + a[0]; } } '
        'return t == 500 ? 42 : 1;'), 42),
    # LKF-444
    'null-conditional store into an owned array': (False, DROPS + MAIN.replace('BODY',
        'owned P[] a = new P[1]; a?[0] = new P(); delete[] a; return 42;'), OWNED_REPLACE),
    'null-conditional store into an owned field': (False, DROPS + HOLDER + 'class Program { static void Set(H q) '
        '{ q?.Item = new P(); } public static int Main() { raii H h = new H(); Set(h); return 42; } }',
        OWNED_REPLACE),
    'owned field replaced twice': (False, DROPS + HOLDER + MAIN.replace('BODY', '{ raii H h = new H(); '
        'h.Item = new P(); h.Item = new P(); } return 40 + P.Drops;'), 42),
    'owned array element replaced twice': (False, DROPS + MAIN.replace('BODY', 'owned P[] a = new P[1]; '
        'a[0] = new P(); a[0] = new P(); delete[] a; return 40 + P.Drops;'), 42),
    'null-conditional store into a plain field': (False, DROPS + 'class H { public P Item; }\nclass Program '
        '{ static void Set(H q, P p) { q?.Item = p; } public static int Main() { raii H h = new H(); '
        'raii P p = new P(); Set(h, p); return 42; } }', 42),
    'null-conditional store into an int array': (False, MAIN.replace('BODY', 'int[] a = new int[1]; a?[0] = 42; '
        'int v = a[0]; delete[] a; return v;'), 42),
    'null-conditional store into a string array': (True, CORE + MAIN.replace('BODY', 'string[] a = new string[1]; '
        'string s = "ab"; a?[0] = s + "c"; int n = a[0].Length; delete[] a; return n + 39;'), 42),
    # LKF-465
    'decimal division keeps the historical scale': (False, MAIN.replace('BODY',
        'decimal a = 10m; decimal b = 4m; decimal c = a / b; return 39 + (int)c;'), 42),
    'decimal equality across scales': (False, MAIN.replace('BODY',
        'decimal a = 1.0m; decimal b = 1.00m; return a == b ? 42 : 1;'), 42),
    'decimal order across scales': (False, MAIN.replace('BODY',
        'decimal a = 1.5m; decimal b = 1.40m; return a > b ? 42 : 1;'), 42),
    'decimal remainder': (False, MAIN.replace('BODY',
        'decimal a = 10m; decimal b = 4m; decimal c = a % b; return c == 2m ? 42 : 1;'), 42),
    # LKF-480
    '128-bit minimum divided by minus one wraps': (False, MAIN.replace('BODY',
        'i128 m = (i128)1 << 127; i128 d = -1; i128 r = m / d; return r == m ? 42 : 1;'), 42),
    '128-bit minimum remainder by minus one': (False, MAIN.replace('BODY',
        'i128 m = (i128)1 << 127; i128 d = -1; i128 r = m % d; return r == 0 ? 42 : 1;'), 42),
    'int minimum divided by minus one wraps': (False, MAIN.replace('BODY',
        'int m = (int)1 << 31; int d = -1; int r = m / d; return r == m ? 42 : 1;'), 42),
}


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=180)


def build_core(compiler, root):
    library_sources = Path(sys.argv[2]).resolve() / 'stdlib'
    libraries = root / 'libraries'
    libraries.mkdir()
    shutil.copytree(library_sources / 'NativeSharp.Core', libraries / 'core', ignore=shutil.ignore_patterns('obj', 'bin'))
    core = run([compiler, '--no-artifact-cache', '--emit-project-oop-lib',
                str(libraries / 'core/NativeSharp.Core.nsproj'), '-o', str(libraries / 'NativeSharpCore.lib')],
               libraries)
    if core.returncode:
        raise RuntimeError('Core library: ' + (core.stdout + core.stderr)[-600:])
    return libraries


def project(core):
    reference = ''
    if core is not None:
        reference = ('<ItemGroup><LibraryReference Include="{0}/NativeSharpCore.lib" '
                     'Metadata="{0}/NativeSharpCore.nsmeta" /></ItemGroup>').format(core.as_posix())
    return ('<Project><PropertyGroup><Name>P</Name><OutputType>Exe</OutputType></PropertyGroup>'
            '<ItemGroup><Compile Include="*.ns" /></ItemGroup>' + reference + '</Project>')


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-lowering-dispatch-') as temporary:
        root = Path(temporary)
        core = build_core(compiler, root)
        for index, (name, (needs_library, source, expected)) in enumerate(CASES.items()):
            folder = root / ('case-' + str(index))
            folder.mkdir()
            files = source if isinstance(source, dict) else {'Program.ns': source}
            for file_name, text in files.items():
                (folder / file_name).write_text(text, encoding='utf-8')
            case = folder / 'Case.nsproj'
            case.write_text(project(core if needs_library else None), encoding='utf-8')
            if isinstance(expected, str):
                result = run([compiler, '--emit-project-oop-exe', str(case), '-o', str(folder / 'rejected.exe'),
                              '--no-artifact-cache', '--no-error-log'], folder)
                output = result.stdout + result.stderr
                print(name, 'exit', result.returncode, flush=True)
                if result.returncode == 0 or expected not in output or 'internal error' in output or \
                        'lowering failed' in output:
                    failures.append((name, 'rejection', output.strip()[-300:]))
                continue
            for level in ('O0', 'O1', 'O2', 'O3'):
                exe = folder / ('case-' + level + '.exe')
                build = run([compiler, '-' + level, '--emit-project-oop-exe', str(case), '-o', str(exe),
                             '--no-artifact-cache', '--no-error-log'], folder)
                if build.returncode:
                    failures.append((name, level, 'build', (build.stdout + build.stderr).strip()[-300:]))
                    continue
                executed = run([str(exe)], folder)
                print(name, level, 'exit', executed.returncode, flush=True)
                if executed.returncode != expected:
                    failures.append((name, level, 'exit', executed.returncode))
    if failures:
        for failure in failures:
            print('FAIL', failure)
        return 1
    print('PASS', len(CASES), 'cases')
    return 0


if __name__ == '__main__':
    sys.exit(main())
