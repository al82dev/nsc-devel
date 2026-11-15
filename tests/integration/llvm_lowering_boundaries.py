"""LLVM lowering at the edge of the supported surface (LKF-436..LKF-499, NSC-NEW-029).

Each program either exits with its expected code at O0 and O2, is rejected with the exact
diagnostic, or has its emitted LLVM IR checked, and every rejection has an accepted control.
Defects fixed with these cases:

- An argument that never completes (`loop { throw ...; }`) of a virtual or interface call, a
  constructor or an array initializer produced invalid IR or asked for the ABI type of `never`
  (LKF-440).
- A packed struct field was read and written with the natural alignment of its type, and an
  aggregate initializer did the same (LKF-436, LKF-445).
- An exception no clause matched left the function even when an enclosing try of the same
  function had a matching catch (LKF-446).
- A using initializer that never completes stored after the terminator (LKF-449).
- A generated expression of 5000 terms overflowed the compiler stack (LKF-457).
- An inline assembly placeholder of twenty digits wrapped around to a valid operand (LKF-460).
- A blocking await of a double, string or struct result did not compile (LKF-471).
- A declared but never assigned iterator local called address zero at scope exit (LKF-473).
- Awaiting a virtual or interface async call ran the base body (LKF-483).
- An async or generator instance method could not use `this` (NSC-NEW-029).
"""
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

CORE = 'using NativeSharp.Core;\n'
ERRORS = ('namespace NativeSharp.Core { public class AErr : Exception { } '
          'public class BErr : Exception { } }\n')
NEVER = 'loop { throw new Exception(); }'
DISPATCH = ('class B { public virtual int F(int x) { return x; } }\ninterface I { int G(int x); }\n'
            'class C : B, I { public override int F(int x) { return x + 1; } public int G(int x) { return x; } }\n'
            'class K { public int V; public K(int v) { V = v; } }\n')


def never_call(body):
    return (CORE + DISPATCH + 'class Program { static int D(int x) { return x; } static int Run(B b, I i, int k) '
            'throws Exception { ' + body + ' } public static int Main() { raii C c = new C(); '
            'try { return Run(c, c, 0); } catch (Exception e) { return 42; } } }')


def nested(body, main=None):
    main = main or ('public static int Main() { try { return Run(); } catch (Exception e) { return 3; } }')
    return CORE + ERRORS + ('class R { public static int Drops = 0; ~R() { Drops = Drops + 1; } }\n'
                            'class Program { static int Log = 0; static int Run() throws Exception { ' + body +
                            ' } ' + main + ' }')


def deep(terms):
    return ('class Program { public static int Main() { int x = 0' + ' + 1' * terms + '; return x - ' +
            str(terms - 42) + '; } }')


def assembly(placeholder):
    return ('public class Program { public static int Main() { int source = 40; int result; asm { mov $out0, ' +
            placeholder + '\n add $out0, 2 } out(result : "={rax}") in(source : "r"); return result; } }')


PACKED = 'pack(1) struct P { public byte A; public long B; public int C; }\n'
GEN = 'static Iterator<int, int> Gen() { yield return 40; yield return 2; } '
VIRTUAL_ASYNC = ('class B { public virtual async int G() { return 1; } }\n'
                 'class C : B { public override async int G() { yield; return 42; } }\n')

CASES = {
    # LKF-440
    'argument that never completes in a direct call': (True, never_call('return D(' + NEVER + ');'), 42),
    'argument that never completes in a virtual call': (True, never_call('return b.F(' + NEVER + ');'), 42),
    'argument that never completes in an interface call': (True, never_call('return i.G(' + NEVER + ');'), 42),
    'argument that never completes in a constructor': (True, never_call('raii K o = new K(' + NEVER + '); '
        'return o.V;'), 42),
    'element that never completes in an array initializer': (True, never_call('int[] a = new int[] { 1, ' + NEVER +
        ' }; int v = a[0]; delete[] a; return v;'), 42),
    'virtual call with completing arguments': (True, never_call('return b.F(k == 1 ? 1 : 2) + 39;'), 42),
    # LKF-436 / LKF-445
    'packed struct initialized and read': (False, PACKED + 'class Program { public static int Main() { '
        'P p = {1, 40, 1}; return (int)p.B + p.A + p.C; } }', 42),
    'packed struct initializer alignment': (False, PACKED + 'class Program { public static int Main() { '
        'P p = {1, 40, 1}; return (int)p.B + p.A + p.C; } }',
        ('ir', [r'store i64 40, ptr %aggregate\.element\.addr\d*, align 1\b',
                r'load i64, ptr %field\.addr\d*, align 1\b'])),
    'packed struct field assignment alignment': (False, PACKED + 'class Program { public static int Main() { '
        'P p = {1, 0, 0}; p.B = 40; p.C = 1; p.B += 0; return (int)p.B + p.A + p.C; } }',
        ('ir', [r'store i64 40, ptr %field\.addr\d*, align 1\b', r'store i32 1, ptr %field\.addr\d*, align 1\b',
                r'%compound\.load = load i64, ptr %field\.addr\d*, align 1\b'])),
    'packed struct inside a class': (False, 'pack(1) struct P { public byte A; public long B; }\n'
        'class H { public P V; }\nclass Program { public static int Main() { raii H h = new H(); h.V.B = 41; '
        'h.V.A = 1; return (int)h.V.B + h.V.A; } }', ('ir', [r'store i64 41, ptr %field\.addr\d*, align 1\b'])),
    'natural struct keeps its field alignment': (False, 'struct N { public byte A; public long B; }\n'
        'class Program { public static int Main() { N n; n.A = 1; n.B = 41; return (int)n.B + n.A; } }', 42),
    # LKF-446
    'unmatched catch reaches an enclosing catch': (True, nested('try { try { throw new BErr(); } '
        'catch (AErr a) { return 1; } } catch (BErr b) { return 42; } return 2;'), 42),
    'unmatched catch runs the inner finally first': (True, nested('try { try { throw new BErr(); } '
        'catch (AErr a) { return 1; } finally { Log = Log + 40; } } catch (BErr b) { return Log + 2; } return 2;'), 42),
    'unmatched catch releases owners inside the outer try': (True, nested('try { raii R r = new R(); '
        'try { throw new BErr(); } catch (AErr a) { return 1; } } catch (BErr b) { return 41 + R.Drops; } return 2;'),
        42),
    'unmatched catch keeps owners outside the outer try': (True, nested('raii R keep = new R(); try { '
        'try { throw new BErr(); } catch (AErr a) { return 1; } } catch (BErr b) { return 42 + R.Drops; } return 4;'),
        42),
    'unmatched catch crosses two levels': (True, nested('try { try { try { throw new BErr(); } '
        'catch (AErr a) { return 1; } } catch (AErr a) { return 2; } } catch (BErr b) { return 42; } return 4;'), 42),
    'unmatched catch runs an outer finally': (True, nested('try { try { try { throw new BErr(); } '
        'catch (AErr a) { return 1; } } finally { Log = Log + 1; } } catch (BErr b) { return 41 + Log; } return 4;'),
        42),
    'unmatched catch inside a catch body': (True, nested('try { try { throw new AErr(); } catch (AErr a) { '
        'try { throw new BErr(); } catch (AErr x) { return 1; } } } catch (BErr b) { return 42; } return 4;'), 42),
    'unmatched catch in a loop': (True, nested('int t = 0; for (int i = 0; i < 1000; i = i + 1) { try { '
        'try { throw new BErr(); } catch (AErr a) { return 1; } } catch (BErr b) { t = t + 1; } } return t - 958;'),
        42),
    'unmatched catch without enclosing catch reaches the caller': (True, nested('try { try { throw new BErr(); } '
        'catch (AErr a) { return 1; } } finally { Log = Log + 1; } return 4;',
        'public static int Main() { try { return Run(); } catch (BErr e) { return 41 + Program.Log; } '
        'catch (Exception e) { return 3; } }'), 42),
    # LKF-447 / LKF-476
    'coroutine frame allocation is checked': (False, 'class Program { public static async int Slow() { yield; '
        'return 7; } public static async int Main() { int r = await Slow(); return r + 35; } }',
        ('ir', [r'coro\.frame\.new\.nonnull'])),
    'generator frame allocation is checked': (True, CORE + 'class Program { static Iterator<int, int> Gen(int n) '
        '{ int i = 0; while (i < n) { yield return i; i = i + 1; } } public static int Main() { '
        'Iterator<int, int> it = Gen(10); int t = 0; while (it.MoveNext()) { t = t + it.Current; } return t - 3; } }',
        ('ir', [r'gen\.frame\.alloc\.nonnull'])),
    'generator with a checked frame runs': (True, CORE + 'class Program { static Iterator<int, int> Gen(int n) '
        '{ int i = 0; while (i < n) { yield return i; i = i + 1; } } public static int Main() { '
        'Iterator<int, int> it = Gen(10); int t = 0; while (it.MoveNext()) { t = t + it.Current; } return t - 3; } }',
        42),
    # LKF-448
    'static local wait pauses': (False, 'class Program { static int Seed() { return 42; } static int Work() '
        '{ static int value = Seed(); return value; } public static int Main() { return Work(); } }',
        ('ir', [r'call void @llvm\.x86\.sse2\.pause\(\)'])),
    'static local initialized once': (False, 'class Program { static int Calls = 0; static int Seed() '
        '{ Calls = Calls + 1; return 41; } static int Work() { static int value = Seed(); return value; } '
        'public static int Main() { int a = Work(); int b = Work(); return a + Calls + b - 41; } }', 42),
    # LKF-449
    'using initializer that never completes': (True, CORE + ERRORS + 'class Res : IDisposable { '
        'public static int Disposed = 0; public void Dispose() { Disposed = Disposed + 1; } }\nclass Program { '
        'static int Run() throws Exception { using (Res r = loop { throw new BErr(); }) { return 1; } } '
        'public static int Main() { try { return Run(); } catch (Exception e) { return 42 + Res.Disposed; } } }', 42),
    # LKF-452
    'destructor cannot declare throws': (False, 'class T { ~T() throws Exception { } }\n'
        'class Program { public static int Main() { return 42; } }', 'destructor declarations require a body'),
    'destructor cannot let an exception escape': (True, CORE + 'class T { static void F() throws Exception '
        '{ throw new Exception(); } ~T() { F(); } }\nclass Program { public static int Main() { return 42; } }',
        'which is neither caught here nor declared by the enclosing callable'),
    # LKF-457
    'expression of 20000 terms': (False, deep(20000), 42),
    'expression of 100000 terms': (False, deep(100000), 42),
    'expression nested 20000 times': (False, 'class Program { public static int Main() { int x = ' + '(' * 20000 +
        '42' + ')' * 20000 + '; return x; } }', 42),
    # LKF-460
    'inline assembly placeholder': (False, assembly('$0'), 42),
    'inline assembly input placeholder that wraps': (False, assembly('$18446744073709551616'),
        'asm input placeholder does not name a declared in operand'),
    'inline assembly output placeholder that wraps': (False, assembly('$0').replace(
        'mov $out0,', 'mov $out18446744073709551616,'), 'asm output placeholder does not name a declared out operand'),
    # LKF-471
    'blocking await of a double': (False, 'class Program { static async double F() { return 41.5; } '
        'public static int Main() { double d = await F(); return (int)(d + 0.5); } }', 42),
    'blocking await of a string': (True, CORE + 'class Program { static async string F() { return "abc"; } '
        'public static int Main() { string d = await F(); return d.Length + 39; } }', 42),
    'blocking await of a struct': (False, 'struct S { public int A; public long B; }\nclass Program { '
        'static async S F() { S s; s.A = 40; s.B = 2; return s; } public static int Main() { S d = await F(); '
        'return d.A + (int)d.B; } }', 42),
    'blocking await of an int': (False, 'class Program { static async int F() { return 42; } '
        'public static int Main() { int d = await F(); return d; } }', 42),
    # LKF-473
    'iterator local never assigned': (True, CORE + 'class Program { public static int Main() { '
        '{ Iterator<int, int> it; } return 42; } }', 42),
    'iterator local assigned on one path': (True, CORE + 'class Program { ' + GEN + 'static int K = 0; '
        'public static int Main() { int t = 2; { Iterator<int, int> it; if (K == 1) { it = Gen(); } } '
        'return t + 40; } }', 42),
    'iterator local used and dropped': (True, CORE + 'class Program { ' + GEN + 'public static int Main() { '
        'Iterator<int, int> it = Gen(); int t = 0; while (it.MoveNext()) { t = t + it.Current; } return t; } }', 42),
    # LKF-483
    'await of a virtual async call': (False, VIRTUAL_ASYNC + 'class Program { static async int Run(B b) { '
        'int r = await b.G(); return r; } public static async int Main() { raii C c = new C(); '
        'int r = await Run(c); return r; } }', 42),
    'await of an abstract async call': (False, 'abstract class B { public abstract async int G(); }\n'
        'class C : B { public override async int G() { yield; return 42; } }\nclass Program { '
        'static async int Run(B b) { int r = await b.G(); return r; } public static async int Main() { '
        'raii C c = new C(); int r = await Run(c); return r; } }', 42),
    'await of a sealed override': (False, 'class B { public virtual async int G() { return 1; } }\n'
        'class C : B { public sealed override async int G() { yield; return 42; } }\nclass Program { '
        'static async int Run(C x) { int r = await x.G(); return r; } public static async int Main() { '
        'raii C c = new C(); int r = await Run(c); return r; } }', 42),
    'blocking await of a virtual call that completes': (False, 'class B { public virtual async int G() '
        '{ return 1; } }\nclass C : B { public override async int G() { return 42; } }\nclass Program { '
        'static int Run(B b) { int r = await b.G(); return r; } public static int Main() { raii C c = new C(); '
        'return Run(c); } }', 42),
    'blocking await of a virtual call that suspends panics': (False, VIRTUAL_ASYNC + 'class Program { '
        'static int Run(B b) { int r = await b.G(); return r; } public static int Main() { raii C c = new C(); '
        'return Run(c); } }', 142),
    'blocking await of a known suspending producer': (False, 'class Program { static async int F() { yield; '
        'return 42; } public static int Main() { var f = F(); int r = await f; return r; } }',
        'synchronous await of a producer that can suspend'),
    # NSC-NEW-029
    'async instance method reads a field': (False, 'class C { public int V = 42; public async int G() '
        '{ return V; } }\nclass Program { public static async int Main() { raii C c = new C(); '
        'int r = await c.G(); return r; } }', 42),
    'async instance method reads a field after a suspension': (False, 'class C { public int V = 42; '
        'public async int G() { yield; return V; } }\nclass Program { public static async int Main() { '
        'raii C c = new C(); int r = await c.G(); return r; } }', 42),
    'async instance method calls through this': (False, 'class C { public int H() { return 42; } '
        'public async int G() { return this.H(); } }\nclass Program { public static async int Main() { '
        'raii C c = new C(); int r = await c.G(); return r; } }', 42),
    'async instance method writes a field and reads a parameter': (False, 'class C { public int V = 40; '
        'public async int G(int k) { yield; V = V + k; return V; } }\nclass Program { public static async int '
        'Main() { raii C c = new C(); int r = await c.G(2); return r == 42 && c.V == 42 ? 42 : 1; } }', 42),
    'async override reads a base field': (False, 'class B { public int V = 40; public virtual async int G() '
        '{ return V; } }\nclass C : B { public override async int G() { yield; return V + 2; } }\nclass Program { '
        'static async int Run(B b) { return await b.G(); } public static async int Main() { raii C c = new C(); '
        'int r = await Run(c); return r; } }', 42),
    'generator instance method reads and writes a field': (True, CORE + 'class C { public int V = 20; '
        'public Iterator<int, int> Gen() { yield return V; V = V + 1; yield return V + 1; } }\nclass Program { '
        'public static int Main() { raii C c = new C(); Iterator<int, int> it = c.Gen(); int t = 0; '
        'while (it.MoveNext()) { t = t + it.Current; } return t; } }', 42),
}


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=600)


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
    return libraries


def project(core):
    reference = ''
    if core is not None:
        reference = ('<ItemGroup><LibraryReference Include="{0}/NativeSharpCore.lib" '
                     'Metadata="{0}/NativeSharpCore.nsmeta" /></ItemGroup>').format(core.as_posix())
    return ('<Project><PropertyGroup><Name>P</Name><OutputType>Exe</OutputType></PropertyGroup>'
            '<ItemGroup><Compile Include="*.ns" /></ItemGroup>' + reference + '</Project>')


def check_ir(compiler, case, folder, patterns, name, failures):
    result = run([compiler, '--dump-project-oop-llvm', str(case)], folder)
    output = result.stdout + result.stderr
    print(name, 'ir exit', result.returncode, flush=True)
    if result.returncode:
        failures.append((name, 'ir', output.strip()[-300:]))
        return
    for pattern in patterns:
        if re.search(pattern, output) is None:
            failures.append((name, 'ir pattern missing', pattern))


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-llvm-lowering-') as temporary:
        root = Path(temporary)
        core = build_core(compiler, root)
        for index, (name, (needs_library, source, expected)) in enumerate(CASES.items()):
            folder = root / ('case-' + str(index))
            folder.mkdir()
            (folder / 'Program.ns').write_text(source, encoding='utf-8')
            case = folder / 'Case.nsproj'
            case.write_text(project(core if needs_library else None), encoding='utf-8')
            if isinstance(expected, tuple):
                check_ir(compiler, case, folder, expected[1], name, failures)
                continue
            if isinstance(expected, str):
                result = run([compiler, '--emit-project-oop-exe', str(case), '-o', str(folder / 'rejected.exe'),
                              '--no-artifact-cache', '--no-error-log'], folder)
                output = result.stdout + result.stderr
                print(name, 'exit', result.returncode, flush=True)
                if result.returncode == 0 or expected not in output or 'internal error' in output or \
                        'lowering failed' in output:
                    failures.append((name, 'rejection', output.strip()[-300:]))
                continue
            for level in ('O0', 'O2'):
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
