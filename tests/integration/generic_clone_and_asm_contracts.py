"""Generic clones, alias expansion, inline assembly, layouts and HIR lowering: LKF-117..LKF-414.

- LKF-358: `new (allocator, failure: null) T(...)` keeps its failure profile inside a generic method.
- LKF-369: an alias of `int[,]` keeps its rank through expansion.
- LKF-128: `out(x : "+r")` reads the previous value (tied input) at O0 and O2.
- LKF-132: an unused trapping division or bounds check is not removed.
- LKF-117, LKF-120: inline array layouts and bit-field widths past their limits are rejected.
- LKF-335: a foreach collection is evaluated once.
- LKF-379: a throw in a sync generator with an error type runs the finally and the raii locals it
  leaves; a throw caught inside the generator reaches its catch.
- LKF-378: a noheap callable rejects an indirect call nested in an expression unless the function type
  is declared noheap.
- LKF-414: an async function returning a pointer can be awaited.
- LKF-372, LKF-373, LKF-375: event invocation with an owned argument, from-end indexes on a
  two-dimensional array and tuple deconstruction behave as declared.
Each program exits with its expected code at O0 and O2 or is rejected with the exact diagnostic.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ALLOC = ('using NativeSharp.Core;\n'
         'class A { public static int Ctors; public int X; public A(int x) { X = x; Ctors = Ctors + 1; } }\n'
         'class NullAllocator : IAllocator {\n'
         '    public byte* Allocate(long bytes, int alignment) { return null; }\n'
         '    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return ptr; }\n'
         '    public void Deallocate(byte* ptr, long bytes, int alignment) { } }\n')
NULL_MAIN = ('public static int Main() { raii NullAllocator na = new NullAllocator(); A fail = MAKE; '
             'return (fail == null && A.Ctors == 0) ? 42 : 1; } }')


P = 'class P { public static int Drops = 0; public int V = 1; ~P() { Drops = Drops + 1; } }\n'
CATCH_MAIN = ('public static int Main() { int t = 0; try { foreach (int x in Gen()) { t = t + x; } } '
              'catch (Exception e) { t = t + 10; } return BASE + t + EXTRA; } }')


def main_body(body):
    return 'class Program { public static int Main() { ' + body + ' } }'


CASES = {
    # LKF-358
    'placement failure profile in a generic method': (True, ALLOC +
        'class Program { static A Make<U>(NullAllocator na, U tag) { return new (na, failure: null) A(9); } ' +
        NULL_MAIN.replace('MAKE', 'Make<int>(na, 1)'), 42),
    'placement failure profile in an ordinary method': (True, ALLOC +
        'class Program { static A Make(NullAllocator na) { return new (na, failure: null) A(9); } ' +
        NULL_MAIN.replace('MAKE', 'Make(na)'), 42),
    # LKF-369
    'alias of a two-dimensional array': (False, 'using Grid = int[,];\n' +
        main_body('Grid g = new int[2, 3]; g[1, 2] = 42; return g[1, 2];'), 42),
    'alias of a two-dimensional array as a parameter': (False, 'using Grid = int[,];\n'
        'class Program { static int Read(Grid g) { return g[1, 2]; } public static int Main() { '
        'int[,] g = new int[2, 3]; g[1, 2] = 42; return Read(g); } }', 42),
    # LKF-128
    'read-write register output': (False, main_body('int x = 40; asm { add $out0, 2 } out(x : "+r"); return x;'),
        42),
    'read-write register output with an input': (False,
        main_body('int x = 40; int y = 2; asm { add $out0, $0 } out(x : "+r") in(y); return x;'), 42),
    'two read-write register outputs': (False, main_body('long x = 40; long y = 1;\n'
        '    asm { add $out0, $0\n          add $out1, $0 } out(x : "+r", y : "+r") in(y); return (int)(x + y);'),
        43),
    # LKF-132
    'unused division by zero still traps': (False, main_body('int z = 0; int unused = 10 / z; return 42;'), 129),
    'unused out-of-range index still traps': (False, 'class Program { static int[] A = new int[2]; '
        'public static int Main() { int i = 5; int unused = A[i]; return 42; } }', 132),
    # LKF-117
    'inline array layout overflow': (False, 'struct Big { public long[1152921504606846976] V; '
        'public long[1152921504606846976] W; }\n' + main_body('return 42;'),
        'struct layout overflows the Windows x64 value-size model'),
    'inline array field overflow': (False, 'struct Huge { public long[2305843009213693952] V; }\n' +
        main_body('return sizeof(Huge) > 0 ? 42 : 1;'),
        'field array layout overflows the Windows x64 object-size model'),
    # LKF-120
    'bit-field wider than its storage': (False, 'struct S { public long A : 65; }\n' + main_body('return 42;'),
        'bit-field width exceeds its storage type'),
    'sixty-four bit bit-field': (False, 'struct S { public long A : 64; public int B : 3; }\n' +
        main_body('S s; s.A = -1; s.B = 2; return s.B == 2 && s.A == -1 ? 42 : 1;'), 42),
    # LKF-335
    'foreach collection evaluated once': (False, 'class Program { static int Calls = 0; static int[] Make() { '
        'Calls = Calls + 1; int[] a = new int[3]; a[0] = 10; a[1] = 20; a[2] = 9; return a; } '
        'public static int Main() { int t = 0; foreach (int x in Make()) { t = t + x; } return t + Calls * 3; } }', 42),
    # LKF-379
    'sync generator throw runs its finally': (True, 'using NativeSharp.Core;\nclass Program { static int Done = 0; '
        'static Iterator<int, Exception> Gen() throws Exception { try { yield return 1; throw new Exception(); } '
        'finally { Done = Done + 1; } } ' + CATCH_MAIN.replace('BASE', '30').replace('EXTRA', 'Done'), 42),
    'sync generator throw destroys its raii local': (True, 'using NativeSharp.Core;\n' + P +
        'class Program { static Iterator<int, Exception> Gen() throws Exception { raii P p = new P(); '
        'yield return 1; throw new Exception(); } ' + CATCH_MAIN.replace('BASE', '30').replace('EXTRA', 'P.Drops'),
        42),
    'sync generator throw caught inside the generator': (True, 'using NativeSharp.Core;\nclass Program { '
        'static int Done = 0; static Iterator<int, Exception> Gen() throws Exception { yield return 1; '
        'try { throw new Exception(); } catch (Exception e) { Done = Done + 1; } yield return 2; } ' +
        CATCH_MAIN.replace('BASE', '38').replace('EXTRA', 'Done'), 42),
    'sync generator finally without a throw': (True, 'using NativeSharp.Core;\nclass Program { static int Done = 0; '
        'static Iterator<int, Exception> Gen() throws Exception { try { yield return 1; yield return 2; } '
        'finally { Done = Done + 1; } } ' + CATCH_MAIN.replace('BASE', '38').replace('EXTRA', 'Done'), 42),
    # LKF-378
    'noheap nested indirect call': (False, 'class Program { static int Two() { return 2; } '
        'public static noheap int Work(fn*() -> int f) { return 40 + f(); } '
        'public static int Main() { return Work(Two); } }',
        'cannot invoke an indirect target without a noheap contract'),
    'noheap nested call through a noheap function type': (False, 'class Program { static noheap int Two() '
        '{ return 2; } public static noheap int Work(fn*() -> int noheap f) { return 40 + f(); } '
        'public static int Main() { return Work(Two); } }', 42),
    'noheap statement call through a noheap function type': (False, 'class Program { static noheap int Two() '
        '{ return 2; } public static noheap int Work(fn*() -> int noheap f) { f(); return 42; } '
        'public static int Main() { return Work(Two); } }', 42),
    # LKF-414
    'await of a pointer result': (False, 'class Program { static async int* Get() { return null; } '
        'public static async int Main() { int* p = await Get(); return p == null ? 42 : 1; } }', 42),
    # LKF-372, LKF-373, LKF-375
    'event invocation with an owned argument': (False, P + 'delegate void H(owned P p);\n'
        'class S { public event H E; public void Fire() { E(new P()); } }\n'
        'class Program { static void Take(owned P p) { delete p; } public static int Main() { '
        'raii S s = new S(); s.E += Take; s.Fire(); return 41 + P.Drops; } }', 42),
    'from-end indexes of a two-dimensional array': (False,
        main_body('int[,] m = new int[2, 3]; m[1, 2] = 42; return m[^1, ^1];'), 42),
    'tuple deconstruction of borrowed references': (False, P + 'class Program { static (P, P) Pair(P x, P y) '
        '{ return (x, y); } public static int Main() { raii P x = new P(); raii P y = new P(); '
        '{ var (a, b) = Pair(x, y); a.V = 41; } return x.V + P.Drops; } }', 41),
}


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=600)


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
    with tempfile.TemporaryDirectory(prefix='nsc-generic-asm-contracts-') as temporary:
        root = Path(temporary)
        core = build_core(compiler, root)
        for index, (name, (needs_library, source, expected)) in enumerate(CASES.items()):
            folder = root / ('case-' + str(index))
            folder.mkdir()
            (folder / 'Program.ns').write_text(source, encoding='utf-8')
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
