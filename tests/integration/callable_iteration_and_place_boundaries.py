"""Callables, iteration, places and returns at the edge of the supported surface (LKF-286..LKF-345).

Each program either exits 42 at O0 and O2 or is rejected with the exact diagnostic, and every
rejection has an accepted control. Defects fixed with these cases:

- A function type lends every parameter, but a fresh object passed to one leaked, and a method
  group bound without comparing ownership: a method with an owned parameter bound to a lending
  target released the caller's object again (heap corruption), an owned return bound to a target
  without an owner leaked, and `owned` written in a function type was dropped silently. A delegate
  declared with an owned return did not hand its result to the caller (LKF-286).
- A delegate or lambda captured by a thread executor child was taken as Send although its type
  does not describe its captured environment (LKF-290).
- `move(it.Current)` stayed blocked after MoveNext advanced to a new item (LKF-310).
- An interpolation handler called its first AppendFormatted for every hole, and AppendLiteral and
  Finish were never checked (LKF-314).
- `end - 1` from a one-past-end pointer kept the sentinel mark and could not be dereferenced
  (LKF-324).
- A declaration `int[] b = a;` did not record the alias, so `delete[] b` with a live reference into
  `a` was accepted (LKF-325).
- `delete[] b.Data` with a live reference returned by a ref method of `b` was accepted (LKF-337).

The other cases pin verified boundaries with their controls.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

MAIN = 'class Program { public static int Main() { BODY } }'
CORE = 'using NativeSharp.Core;\n'
ATOMIC = 'using NativeSharp.Core.Threading;\n'
DROPS = ('class Payload { public static int Drops = 0; public int Value; public Payload(int v) { Value = v; } '
         '~Payload() { Drops = Drops + 1; } }\n')
READ = 'static int Read(Payload p) { return p.Value; } '
TAKE = 'static int Take(owned Payload p) { int v = p.Value; delete p; return v; } '
MAKE = 'static owned Payload Make() { return new Payload(41); } '
EXEC = ('using NativeSharp.Core;\nstruct ThreadExec : IThreadExecutor { public int spawned; '
        'public ThreadExec() { spawned = 0; } public int Spawn(Future<void> task) { spawned = spawned + 1; '
        'return spawned - 1; } public int PollJoin(int handle) { return 0; } public void Cancel(int handle) { } }\n')
SCOPE = 'TaskScope<ThreadExec> s = TaskScope<ThreadExec>.Create(new ThreadExec()); '
RES = ('struct Res { public int V; public Res(int v) { V = v; } ~Res() { Drops = Drops + 1; } '
       'public static int Drops = 0; }\n')
GEN = 'static Iterator<Res, int> Gen() { yield return new Res(40); yield return new Res(2); } '
LITERAL = 'public void AppendLiteral(string s) { N = N + 1; } '
INT_HOLE = 'public void AppendFormatted(int v) { N = N + v; } '
BOOL_HOLE = 'public void AppendFormatted(bool v) { N = N + 100; } '
FINISH = 'public H Finish() { return this; } '
BOX = 'class Box { public int[] Data = new int[3]; public ref int At(int i) { return ref Data[i]; } }\n'
INNER = 'class Inner { public int V = 42; }\n'
LEND = 'a parameter of a function type, which lends every parameter'
NOT_SEND = 'future submitted to a thread executor is not Send'
TWICE = 'cannot move from the iterator item twice'
SENTINEL = 'a one-past-end pointer is a comparison/iteration sentinel'
DISPOSABLE = ('class Res : IDisposable { public static int Disposed = 0; '
              'public void Dispose() { Disposed = Disposed + 1; } }\n')
MAYBE = ('static void Maybe(bool f) throws Exception { if (f) { throw new Exception(); } } '
         'static void Consume(owned Payload p) { delete p; } ')
PASS = 'public static borrowed Inner Pass(borrowed Inner x) { return x; } '
PARENT = 'class Parent<T> where T : class { public int V; public Parent(int v) { V = v; } }\n'

CASES = {
    # LKF-286
    'new passed to a function pointer parameter': (False, DROPS + 'class Program { ' + READ +
        'public static int Main() { fn*(Payload) -> int f = Read; return f(new Payload(42)); } }', LEND),
    'new passed to a function reference parameter': (False, DROPS + 'class Program { ' + READ +
        'public static int Main() { fn&(Payload) -> int f = Read; return f(new Payload(42)); } }', LEND),
    'new passed to an inferred lambda': (False, DROPS + MAIN.replace('BODY',
        'var f = (Payload p) => p.Value; return f(new Payload(42));'), LEND),
    'owner lent to a function pointer': (False, DROPS + 'class Program { ' + READ +
        'public static int Main() { fn*(Payload) -> int f = Read; int v = 0; { raii Payload p = new Payload(41); '
        'v = f(p); } return v + Payload.Drops; } }', 42),
    'owned-parameter method bound to a function pointer': (False, DROPS + 'class Program { ' + TAKE +
        'public static int Main() { fn*(Payload) -> int f = Take; raii Payload p = new Payload(41); return f(p); } }',
        "parameter 'p' is owned by the method and lent by the function type"),
    'owned-returning method bound to a function pointer': (False, DROPS + 'class Program { ' + MAKE +
        'public static int Main() { fn*() -> Payload f = Make; Payload p = f(); return p.Value; } }',
        'the method returns an owner and the function type returns no owner'),
    'owned parameter written in a function type': (False, DROPS + 'class Program { ' + READ +
        'public static int Main() { fn*(owned Payload) -> int f = Read; return 42; } }',
        'a function type parameter cannot be owned'),
    'owned-parameter method bound to a lending delegate': (False, DROPS + 'delegate int D(Payload p);\n'
        'class Program { ' + TAKE + 'public static int Main() { D f = Take; raii Payload p = new Payload(41); '
        'return f(p); } }', "parameter 'p' is owned by the method and lent by delegate 'D'"),
    'owned-parameter method bound to an owned delegate': (False, DROPS + 'delegate int D(owned Payload p);\n'
        'class Program { ' + TAKE + 'public static int Main() { D f = Take; return f(new Payload(41)) + '
        'Payload.Drops; } }', 42),
    'owned return bound to a delegate without an owner': (False, DROPS + 'delegate Payload D();\n'
        'class Program { ' + MAKE + 'public static int Main() { D f = Make; Payload p = f(); return p.Value; } }',
        "the method returns an owner and delegate 'D' returns no owner"),
    'owned delegate result held by a raii local': (False, DROPS + 'delegate owned Payload D();\n'
        'class Program { ' + MAKE + 'public static int Main() { int v = 0; { D f = Make; raii Payload p = f(); '
        'v = p.Value; } return v + Payload.Drops; } }', 42),
    'discarded owned delegate result': (False, DROPS + 'delegate owned Payload D();\n'
        'class Program { ' + MAKE + 'public static int Main() { D f = Make; f(); return 41 + Payload.Drops; } }', 42),
    'new passed to a lending delegate': (False, DROPS + 'delegate int D(Payload p);\nclass Program { ' + READ +
        'public static int Main() { D f = Read; return f(new Payload(42)); } }', "of delegate 'D', which is not owned"),
    # LKF-290
    'delegate captured by a thread executor child': (True, EXEC + 'delegate int D();\nclass Program { '
        'static async void Child(ref D v) { } public static int Main() { D d = () => 42; ' + SCOPE +
        's.Spawn(Child(ref d)); s.Join(); return 42; } }', NOT_SEND + ": capture 'd'"),
    'function pointer captured by a thread executor child': (True, EXEC + 'class Program { static int F(int v) '
        '{ return v; } static async void Child(ref fn*(int) -> int v) { } public static int Main() { '
        'fn*(int) -> int f = F; ' + SCOPE + 's.Spawn(Child(ref f)); s.Join(); return 42; } }', 42),
    'pointer in a generic struct captured by a thread executor child': (True, EXEC + 'struct Box<T> { public T V; }\n'
        'class Program { static async void Child(ref Box<int*> v) { } public static int Main() { Box<int*> b; ' +
        SCOPE + 's.Spawn(Child(ref b)); s.Join(); return 42; } }', NOT_SEND),
    'pointer array captured by a thread executor child': (True, EXEC + 'class Program { static async void '
        'Child(ref int*[2] v) { } public static int Main() { int*[2] t; ' + SCOPE + 's.Spawn(Child(ref t)); '
        's.Join(); return 42; } }', NOT_SEND),
    'int captured by a thread executor child': (True, EXEC + 'class Program { static async void Child(ref int v) '
        '{ } public static int Main() { int x = 42; ' + SCOPE + 's.Spawn(Child(ref x)); s.Join(); return x; } }', 42),
    # LKF-301
    'volatile pointer local': (False, MAIN.replace('BODY', 'int x = 42; volatile int* p = &x; return *p;'), 42),
    # LKF-310
    'move of the iterator item after MoveNext advances': (True, CORE + RES + 'class Program { ' + GEN +
        'public static int Main() { Iterator<Res, int> it = Gen(); int total = 0; if (it.MoveNext()) { '
        'Res a = move(it.Current); total = a.V; } if (it.MoveNext()) { Res b = move(it.Current); '
        'total = total + b.V; } return total; } }', 42),
    'move of each iterator item in a loop': (True, CORE + RES + 'class Program { ' + GEN +
        'public static int Main() { int total = 0; { Iterator<Res, int> it = Gen(); while (it.MoveNext()) { '
        'Res r = move(it.Current); total = total + r.V; } } return total + 2 - Res.Drops; } }', 42),
    'second move of the same iterator item': (True, CORE + RES + 'class Program { ' + GEN +
        'public static int Main() { Iterator<Res, int> it = Gen(); int t = 0; if (it.MoveNext()) { '
        'Res a = move(it.Current); Res b = move(it.Current); t = a.V + b.V; } return t; } }', TWICE),
    'second move after a MoveNext that may not run': (True, CORE + RES + 'class Program { ' + GEN +
        'public static int Main() { Iterator<Res, int> it = Gen(); int t = 0; bool c = false; if (it.MoveNext()) '
        '{ Res a = move(it.Current); t = a.V; if (c) { it.MoveNext(); } Res b = move(it.Current); t = t + b.V; } '
        'return t; } }', TWICE),
    # LKF-314
    'interpolation handler overloads in declaration order': (False, 'struct H { public int N; ' + LITERAL +
        BOOL_HOLE + INT_HOLE + FINISH + '}\n' + MAIN.replace('BODY', 'H h = $"a{41}{true}"; return h.N - 100;'),
        42),
    'interpolation handler overloads in reverse order': (False, 'struct H { public int N; ' + LITERAL +
        INT_HOLE + BOOL_HOLE + FINISH + '}\n' + MAIN.replace('BODY', 'H h = $"a{41}{true}"; return h.N - 100;'),
        42),
    'interpolation handler without an overload for a hole': (False, 'struct H { public int N; ' + LITERAL +
        INT_HOLE + FINISH + '}\n' + MAIN.replace('BODY', 'H h = $"a{true}"; return 42;'),
        "no AppendFormatted of interpolation handler takes a hole of type 'builtin:bool'"),
    'interpolation handler Finish with another result': (False, 'struct H { public int N; ' + LITERAL +
        INT_HOLE + 'public int Finish() { return N; } }\n' + MAIN.replace('BODY', 'H h = $"a{41}"; return h.N;'),
        'an interpolation handler Finish must take no parameters and return the handler type'),
    'interpolation handler AppendLiteral without a string': (False, 'struct H { public int N; '
        'public void AppendLiteral(int s) { } ' + INT_HOLE + FINISH + '}\n' +
        MAIN.replace('BODY', 'H h = $"a{42}"; return h.N;'),
        'an interpolation handler AppendLiteral must take exactly one string parameter'),
    # LKF-320
    'union read through another member': (False, 'union Word { public int Signed; public uint Unsigned; }\n' +
        MAIN.replace('BODY', 'Word w; w.Signed = -1; return w.Unsigned == 4294967295 ? 42 : 1;'), 42),
    # LKF-324
    'one-past-end pointer stepped back': (False, MAIN.replace('BODY', 'int[3] a; a[2] = 42; unsafe("t") { '
        'int* end = &a[3]; int* last = end - 1; return *last; }'), 42),
    'one-past-end pointer dereferenced': (False, MAIN.replace('BODY', 'int[3] a; unsafe("t") { int* end = &a[3]; '
        'return *end; }'), SENTINEL),
    'one-past-end pointer plus zero': (False, MAIN.replace('BODY', 'int[3] a; unsafe("t") { int* end = &a[3]; '
        'int* q = end + 0; return *q; }'), SENTINEL),
    'iteration up to the one-past-end pointer': (False, MAIN.replace('BODY', 'int[3] a; a[0] = 40; a[1] = 2; '
        'a[2] = 0; int t = 0; unsafe("t") { int* end = &a[3]; int* p = &a[0]; while (p != end) { t = t + *p; '
        'p = p + 1; } } return t;'), 42),
    # LKF-325
    'delete of an alias while a reference into the array is alive': (False, MAIN.replace('BODY',
        'int[] a = new int[2]; int[] b = a; ref int x = ref a[0]; delete[] b; return x;'),
        "cannot delete[] array 'b' while reference 'x' to one of its elements is alive"),
    'delete of a conditional alias while a reference is alive': (False, MAIN.replace('BODY',
        'int[] a = new int[2]; int[] c = new int[2]; bool k = true; int[] b = k ? a : c; ref int x = ref a[0]; '
        'delete[] b; return x;'), "cannot delete[] array 'b' while reference 'x'"),
    'delete of an alias after the reference ends': (False, MAIN.replace('BODY',
        'int[] a = new int[2]; int[] b = a; ref int x = ref a[0]; x = 42; int v = x; delete[] b; return v;'), 42),
    'delete of another array while a reference is alive': (False, MAIN.replace('BODY',
        'int[] a = new int[2]; int[] b = new int[2]; ref int x = ref a[0]; x = 42; delete[] b; int v = x; '
        'delete[] a; return v;'), 42),
    # LKF-327
    'match guard with a call evaluated once': (False, 'class Program { static int Calls = 0; static bool Check(int v) '
        '{ Calls = Calls + 1; return v > 5; } public static int Main() { int k = 3; int r = match (k) { '
        'var v when Check(v) => 1, _ => 40 }; return r + Calls + 1; } }', 42),
    # LKF-328
    'using over a disposable class': (True, CORE + DISPOSABLE + MAIN.replace('BODY',
        'using (Res r = new Res()) { using (Res q = new Res()) { } } return 40 + Res.Disposed;'), 42),
    'using disposes when the body throws': (True, CORE + DISPOSABLE + 'class Program { static void Boom() throws '
        'Exception { throw new Exception(); } public static int Main() { try { using (Res r = new Res()) { Boom(); } '
        '} catch (Exception e) { } return 41 + Res.Disposed; } }', 42),
    'using over a class that is not disposable': (True, CORE + 'class Res { }\n' + MAIN.replace('BODY',
        'using (Res r = new Res()) { } return 42;'), 'must implement NativeSharp.Core.IDisposable'),
    'struct implementing an interface for using': (True, CORE + 'struct Res : IDisposable { public void Dispose() '
        '{ } }\n' + MAIN.replace('BODY', 'using (Res r = new Res()) { } return 42;'),
        'a NativeSharp 3 struct may implement traits only'),
    # LKF-329
    'recursive local function': (False, MAIN.replace('BODY', 'int Fact(int v) { return v <= 1 ? 1 : v * Fact(v - 1); } '
        'return Fact(4) + 18;'), 42),
    # LKF-330
    'throw of a named local exception': (True, CORE + 'class Program { static void Throw() throws Exception { '
        'Exception e = new Exception(); throw e; } public static int Main() { try { Throw(); } '
        'catch (Exception x) { return 42; } return 1; } }', 'throw hands the exception object to the catch'),
    'throw of a moved local exception': (True, CORE + 'class Program { static void Throw() throws Exception { '
        'Exception e = new Exception(); throw move(e); } public static int Main() { try { Throw(); } '
        'catch (Exception x) { return 42; } return 1; } }', 42),
    # LKF-331
    'owner moved after a call that throws': (True, CORE + DROPS + 'class Program { ' + MAYBE +
        'public static int Main() { { raii Payload p = new Payload(1); try { Maybe(true); Consume(move(p)); } '
        'catch (Exception x) { } } return 41 + Payload.Drops; } }', 42),
    'owner moved before a call that throws': (True, CORE + DROPS + 'class Program { ' + MAYBE +
        'public static int Main() { { raii Payload p = new Payload(1); try { Consume(move(p)); Maybe(true); } '
        'catch (Exception x) { } } return 41 + Payload.Drops; } }', 42),
    # LKF-332
    'variable-length array of strings': (True, CORE + MAIN.replace('BODY', 'int n = 2; string[n] a; string s = "ab"; '
        'a[0] = s + "c"; return a[0].Length + 39;'), 42),
    'variable-length array with an initializer': (False, MAIN.replace('BODY',
        'int n = 3; int[n] a = new int[3]; return 42;'),
        'a variable-length array declaration cannot have an initializer'),
    'variable-length array of a destructible struct': (False, 'struct R { public int V; ~R() { } }\n' +
        MAIN.replace('BODY', 'int n = 2; R[n] a; return 42;'),
        'a variable-length array requires an element type with no destructor and no owned fields'),
    'variable-length array with a bool dimension': (False, MAIN.replace('BODY', 'bool n = true; int[n] a; return 42;'),
        'every variable-length array dimension must have an integral type'),
    'pointer into a variable-length array stored in a static': (False, 'class Program { static int* Keep; '
        'public static int Main() { int n = 3; int[n] a; unsafe("t") { Keep = a; } return 42; } }',
        'pointer into variable-length array'),
    # LKF-337
    'delete of a field array behind a live ref method result': (False, BOX + MAIN.replace('BODY',
        'raii Box b = new Box(); ref int r = ref b.At(1); delete[] b.Data; b.Data = null; return r;'),
        "cannot delete[] 'b.Data' while reference 'r'"),
    'delete of a field array behind a live element reference': (False, BOX + MAIN.replace('BODY',
        'raii Box b = new Box(); ref int r = ref b.Data[1]; delete[] b.Data; b.Data = null; return r;'),
        "cannot delete[] 'b.Data' while reference 'r'"),
    'delete of a field array after the reference ends': (False, BOX + MAIN.replace('BODY',
        'raii Box b = new Box(); ref int r = ref b.At(1); r = 42; int v = r; delete[] b.Data; b.Data = null; '
        'return v;'), 42),
    # LKF-338
    'ref return of a static field': (False, 'class G { public static int V = 42; public static ref int Get() '
        '{ return ref V; } }\n' + MAIN.replace('BODY', 'return G.Get();'), 42),
    'ref return of a local': (False, 'class G { public static ref int Get() { int x = 42; return ref x; } }\n' +
        MAIN.replace('BODY', 'return G.Get();'), 'ref return cannot expose storage owned by the current stack frame'),
    'ref return with a named lifetime': (False, 'class G { public static ref[a] int Pick<lifetime a>(ref[a] int x) '
        '{ return ref x; } }\n' + MAIN.replace('BODY', 'int x = 42; return G.Pick(ref x);'), 42),
    # LKF-339
    'borrowed return forwarded through calls': (False, INNER + 'class G { ' + PASS + 'public static borrowed Inner '
        'Twice(borrowed Inner x) { return Pass(x); } }\n' +
        MAIN.replace('BODY', 'raii Inner i = new Inner(); return G.Twice(i).V;'), 42),
    'borrowed return of a call over a local owner': (False, INNER + 'class G { ' + PASS + 'public static borrowed '
        'Inner Bad() { raii Inner x = new Inner(); return Pass(x); } }\n' + MAIN.replace('BODY', 'return G.Bad().V;'),
        "borrowed return value outlives local owner 'x'"),
    'borrowed return choosing between parameters': (False, INNER + 'class G { public static borrowed Inner Pick(bool '
        'c, borrowed Inner a, borrowed Inner b) { return c ? a : b; } }\n' + MAIN.replace('BODY',
        'raii Inner i = new Inner(); raii Inner j = new Inner(); return G.Pick(true, i, j).V;'), 42),
    'borrowed return choosing a local owner': (False, INNER + 'class G { public static borrowed Inner Pick(bool c, '
        'borrowed Inner a) { raii Inner x = new Inner(); return c ? a : x; } }\n' + MAIN.replace('BODY',
        'raii Inner i = new Inner(); return G.Pick(false, i).V;'), 'borrowed return requires a borrowed value'),
    'borrowed return of an owned field': (False, INNER + 'class Box { public owned Inner I = new Inner(); '
        'public borrowed Inner Get() { return I; } }\n' + MAIN.replace('BODY', 'raii Box b = new Box(); '
        'return b.Get().V;'), 'borrowed return requires a borrowed value'),
    # LKF-340 / LKF-341
    '128-bit constant shift': (False, MAIN.replace('BODY', 'const i128 big = (i128)1 << 100; i128 x = big >> 100; '
        'return (int)x + 41;'), 42),
    '128-bit constant product': (False, MAIN.replace('BODY', 'const i128 a = 9223372036854775807; '
        'const i128 b = a * 4; return b / 4 == a ? 42 : 1;'), 42),
    'constant float to int cast': (False, MAIN.replace('BODY', 'const int k = (int)42.9; return k;'), 42),
    # LKF-343
    'struct default construction with private constructors': (False, 'struct S { public int V; private S(int v) '
        '{ V = v; } }\n' + MAIN.replace('BODY', 'S s = new S(); return s.V + 42;'), 42),
    # LKF-344
    'base initializer of a constrained generic': (False, 'class Payload { }\n' + PARENT + 'class Child : '
        'Parent<Payload> { public Child() : base(42) { } }\n' + MAIN.replace('BODY', 'Child c = new Child(); '
        'return c.V;'), 42),
    'base initializer of a violated constraint': (False, PARENT + 'class Child : Parent<int> { public Child() : '
        'base(42) { } }\n' + MAIN.replace('BODY', 'Child c = new Child(); return c.V;'),
        "requires a reference type ('where T : class')"),
    # LKF-345
    'two independent constructor cycles': (False, 'class A { public int V; public A() : this(1) { } '
        'public A(int v) : this() { } public A(long v) : this(v, 1) { } public A(long v, int w) : this(v) { } }\n' +
        MAIN.replace('BODY', 'return 42;'), 'constructor delegation cycle detected'),
}


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
    return libraries


def project(core, atomic):
    reference = ''
    if core is not None:
        reference = ('<ItemGroup><LibraryReference Include="{0}/NativeSharpCore.lib" '
                     'Metadata="{0}/NativeSharpCore.nsmeta" /></ItemGroup>').format(core.as_posix())
    sources = '<Compile Include="Program.ns" />' + ('<Compile Include="Atomics.ns" />' if atomic else '')
    return ('<Project><PropertyGroup><Name>P</Name><OutputType>Exe</OutputType></PropertyGroup>'
            '<ItemGroup>' + sources + '</ItemGroup>' + reference + '</Project>')


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    repo = Path(__file__).resolve().parent.parent.parent
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-callable-place-') as temporary:
        root = Path(temporary)
        core = build_core(compiler, root)
        for index, (name, (needs_library, source, expected)) in enumerate(CASES.items()):
            folder = root / ('case-' + str(index))
            folder.mkdir()
            (folder / 'Program.ns').write_text(source, encoding='utf-8')
            atomic = source.startswith(ATOMIC)
            if atomic:
                shutil.copyfile(repo / 'lib/NativeSharp.Core.Threading/Atomics.ns', folder / 'Atomics.ns')
            case = folder / 'Case.nsproj'
            case.write_text(project(core if needs_library and not atomic else None, atomic), encoding='utf-8')
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
