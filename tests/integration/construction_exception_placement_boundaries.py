"""Failed construction, nested exceptions, noheap promotion, placement deletes and stackalloc warnings.

Fichas LKF-020, LKF-022, LKF-023, LKF-024, LKF-107, LKF-108, LKF-109, LKF-110 and LKF-111. Core is built
from the repository first. Each program either exits with the expected code at O0 and O2 or is rejected
with the exact diagnostic. Two defects were fixed with these cases: `delete` of an object constructed
with `new (storage) T()` over raw storage handed stack memory to the heap (heap corruption) and is now
rejected (LKF-020/023), and a warning such as "stackalloc inside a loop" was printed only by the
check actions, never by a successful build (LKF-108).
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

USING = 'using NativeSharp.Core;\n'
R = '''class R { public static int Drops = 0; public int V; public R(int v) { V = v; } ~R() { Drops = Drops + 1; } }
'''
BOOM = '''class Boom : Exception { public static int Drops = 0; public int Code; public Boom(int c) { Code = c; }
 ~Boom() { Drops = Drops + 1; } }
'''
POOL = '''class Pool : IAllocator { private owned byte[] pool; private int next; public static int Allocs = 0;
 public static int Frees = 0; public Pool(int size) { pool = new byte[size]; next = 0; }
 public byte* Allocate(long bytes, int alignment) { Allocs = Allocs + 1; byte* data = Spans.Of<byte>(pool).Data();
 long a = (long)data; long s = (a + 63) / 64 * 64 + (long)next * 256; next = next + 1; return data + (s - a); }
 public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return null; }
 public void Deallocate(byte* ptr, long bytes, int alignment) { Frees = Frees + 1; } }
'''
CELL = 'class Cell { public static int D = 0; public int L; ~Cell() { D = D + 1; } }\n'
LOOP_WARNING = 'stackalloc inside a loop reserves again on every iteration'

# Each case: (source, expected exit code or diagnostic, text the successful build must print or None).
CASES = {
    'constructor throws after initializing an owned field': (USING + R + BOOM + '''class C { public owned R f;
 public C(bool fail) throws Boom { f = new R(1); if (fail) { throw new Boom(1); } } }
class Program { public static int Main() { try { C c = new C(true); return 1; } catch (Boom b) { }
 return R.Drops * 10 + Boom.Drops + 31; } }''', 42, None),
    'base constructor throws before the derived body': (USING + R + BOOM + '''class B { public owned R bf;
 public B(bool fail) throws Boom { bf = new R(1); if (fail) { throw new Boom(1); } } }
class D : B { public owned R df; public D(bool fail) throws Boom : base(fail) { df = new R(2); } }
class Program { public static int Main() { try { D d = new D(true); return 1; } catch (Boom b) { }
 return R.Drops * 10 + Boom.Drops + 31; } }''', 42, None),
    'derived constructor throws after the base completed': (USING + R + BOOM + '''class B { public static int Dtors = 0;
 public owned R bf; public B() { bf = new R(1); } ~B() { Dtors = Dtors + 1; } }
class D : B { public owned R df; public D(bool fail) throws Boom : base() { df = new R(2);
 if (fail) { throw new Boom(1); } } }
class Program { public static int Main() { try { D d = new D(true); return 1; } catch (Boom b) { }
 return R.Drops * 10 + B.Dtors + 21; } }''', 42, None),
    'allocator construction that throws returns its block': (USING + R + BOOM + POOL + '''class C { public owned R f;
 public C(bool fail) throws Boom { f = new R(1); if (fail) { throw new Boom(1); } } }
class Program { public static int Main() { raii Pool p = new Pool(4096); try { owned C c = new (p) C(true); return 1; }
 catch (Boom b) { } return Pool.Allocs * 10 + Pool.Frees + R.Drops * 20 + 11; } }''', 42, None),
    'field initializer that throws': (USING + R + BOOM + '''class T { public static int Fail() throws Boom {
 throw new Boom(2); } }
class C { public owned R f = new R(1); public int g = T.Fail(); public C() throws Boom { } }
class Program { public static int Main() { return 42; } }''',
        "'Fail' throws 'type:P::Boom', which is neither caught", None),
    'throw inside a catch handler': (USING + BOOM + '''class Program { public static int Main() { int code = 0;
 try { try { throw new Boom(1); } catch (Boom e) { throw new Boom(e.Code + 1); } } catch (Boom e2) { code = e2.Code; }
 return Boom.Drops * 10 + code + 20; } }''', 42, None),
    'rethrow keeps the exception object': (USING + BOOM + '''class Program { public static int Main() { int code = 0;
 try { try { throw new Boom(7); } catch (Boom e) { throw; } } catch (Boom e2) { code = e2.Code; }
 return Boom.Drops * 10 + code + 25; } }''', 42, None),
    'finally blocks run inner to outer': (USING + BOOM + '''class Program { static int Trace; public static int Main() {
 try { try { throw new Boom(1); } finally { Trace = Trace * 10 + 1; } } catch (Boom e) { Trace = Trace * 10 + 2; }
 finally { Trace = Trace * 10 + 3; } return Trace - 81; } }''', 42, None),
    'noheap method with a promoted raii class': ('''class Cell { public int L; public int R; }
class Program { public static noheap int Work() { raii Cell c = new Cell(); c.L = 40; c.R = 2; return c.L + c.R; }
 public static int Main() { return Work(); } }''', 42, None),
    'noheap method with a local bound to new': ('''class Cell { public int L; }
class Program { public static noheap int Work() { Cell c = new Cell(); c.L = 42; return c.L; }
 public static int Main() { return Work(); } }''', 42, None),
    'noheap method whose object escapes': ('''class Cell { public int L; }
class Program { static noheap owned Cell Make() { Cell c = new Cell(); return move(c); }
 public static int Main() { raii Cell c = Make(); return 42; } }''',
        "NS3030: noheap callable 'Make' cannot contain direct heap allocation", None),
    'delete of an object placed over stack storage': (CELL + '''class Program { public static int Main() {
 long* words = stackalloc long[2]; Cell* c = (Cell*)words; Cell x = new (c) Cell(); delete x; return 42; } }''',
        "'x' was constructed in place with new (storage) and was not allocated with new", None),
    'delete of a struct placed over stack storage': ('''struct S { public int L; }
class Program { public static int Main() { S* st = stackalloc S[1]; S* s = new (st) S(); delete s; return 42;
 } }''',
        "'s' was constructed in place with new (storage)", None),
    'destroy of an object placed over stack storage': (CELL + '''class Program { public static int Main() {
 long* words = stackalloc long[2]; Cell* c = (Cell*)words; Cell x = new (c) Cell(); destroy x;
 return Cell.D + 41; } }''',
        42, None),
    'allocator placement released through the allocator': (USING + POOL + '''class C { public int V; }
class Program { public static int Main() { raii Pool p = new Pool(4096); { owned C c = new (p) C(); c.V = 1; }
 return Pool.Allocs * 10 + Pool.Frees * 20 + 12; } }''', 42, None),
    'stackalloc in a loop warns in a successful build': ('''class Program { public static int Main() { int s = 0;
 int i = 0; while (i < 4) { int* p = stackalloc int[4]; p[0] = i; s = s + p[0]; i = i + 1; } return s + 36; } }''',
        42, LOOP_WARNING),
    'stackalloc outside a loop does not warn': ('''class Program { public static int Main() { int s = 0;
 int* p = stackalloc int[4]; int i = 0; while (i < 4) { p[0] = i; s = s + p[0]; i = i + 1; } return s + 36; } }''',
        42, ''),
}

PROJECT = ('<Project><PropertyGroup><Name>P</Name><OutputType>Exe</OutputType></PropertyGroup>'
           '<ItemGroup><Compile Include="Program.ns" />{}</ItemGroup></Project>')
REFERENCE = '<LibraryReference Include="{0}/NativeSharpCore.lib" Metadata="{0}/NativeSharpCore.nsmeta" />'


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=180)


def build_core(compiler, root):
    repo = Path(__file__).resolve().parent.parent.parent
    libraries = root / 'libraries'
    libraries.mkdir()
    shutil.copytree(repo / 'lib/NativeSharp.Core', libraries / 'core', ignore=shutil.ignore_patterns('obj', 'bin'))
    core = run([compiler, '--no-artifact-cache', '--emit-project-oop-lib',
                str(libraries / 'core/NativeSharp.Core.nsproj'), '-o', str(libraries / 'NativeSharpCore.lib')],
               libraries)
    if core.returncode:
        raise RuntimeError('Core library: ' + (core.stdout + core.stderr)[-600:])
    return libraries.as_posix()


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-construction-placement-') as temporary:
        root = Path(temporary)
        os.environ['TEMP'] = os.environ['TMP'] = temporary
        libraries = build_core(compiler, root)
        for index, (name, (source, expected, printed)) in enumerate(CASES.items()):
            folder = root / ('case-' + str(index))
            folder.mkdir()
            (folder / 'Program.ns').write_text(source, encoding='utf-8')
            project = folder / 'Case.nsproj'
            project.write_text(PROJECT.format(REFERENCE.format(libraries) if source.startswith('using') else ''),
                               encoding='utf-8')
            if isinstance(expected, str):
                result = run([compiler, '--emit-project-oop-exe', str(project), '-o', str(folder / 'rejected.exe'),
                              '--no-artifact-cache', '--no-error-log'], folder)
                output = result.stdout + result.stderr
                print(name, 'exit', result.returncode, flush=True)
                if result.returncode == 0 or expected not in output:
                    failures.append((name, 'rejection', output.strip()[-300:]))
                continue
            for level in ('O0', 'O2'):
                exe = folder / ('case-' + level + '.exe')
                build = run([compiler, '-' + level, '--emit-project-oop-exe', str(project), '-o', str(exe),
                             '--no-artifact-cache', '--no-error-log'], folder)
                output = build.stdout + build.stderr
                if build.returncode:
                    failures.append((name, level, 'build', output.strip()[-300:]))
                    continue
                if printed is not None and ((printed and printed not in output) or
                                            (not printed and LOOP_WARNING in output)):
                    failures.append((name, level, 'build output', output.strip()[-300:]))
                executed = run([str(exe)], folder)
                print(name, level, 'exit', executed.returncode, flush=True)
                if executed.returncode != expected:
                    failures.append((name, level, 'exit', executed.returncode, 'expected', expected))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('construction_exception_placement_boundaries: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
