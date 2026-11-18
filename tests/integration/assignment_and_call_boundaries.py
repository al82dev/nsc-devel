"""Assignment and call forms at the edge of the supported surface (LKF-277..LKF-291).

Each program either exits 42 at O0, O1, O2 and O3 or is rejected with the exact diagnostic, and every
rejection has an accepted control. Defects fixed with these cases:

- `a[i] ??= "text"` stored a string literal into a plain string array without marking the slot as
  borrowed, so a later `delete[] a` freed the literal (heap corruption, LKF-277).
- `p?.Field = value` skipped the implicit conversion check: a string or a narrowing long reached an
  int field (LKF-280).
- A simple type name with three or more candidates in other namespaces alternated the fallback and
  silently picked the last candidate (LKF-281).
- Atomic operations read their arguments by position, so named or reordered arguments validated and
  executed the wrong operands; they are now rejected with an explanation (LKF-283).
- `p?.M()` rejected an `Optional<T>` result although its diagnostic recommends Optional<T>; the empty
  Optional is now the result of a skipped call (LKF-291).
- Passing a fresh array to a call marked every array local as aliasing unknown storage before the
  call's own view checks, so it conflicted with an older view of unknown root (NSC-NEW-018).
- A TaskScope Join cleared every scoped child borrow, whichever scope or type it belonged to, and
  Spawn saw only a direct child call matched by position (LKF-289).
- A collection builder used the first declared Add for every element (LKF-298).
- At O0 the zeroing of a large fixed-size local crashed the compiler (LKF-293).
- stackalloc and raw new looked only at the element's direct fields (LKF-302).
- A conditional over a class and its base was rejected (LKF-312).
- `wrapping`/`unchecked` were rejected for every type with a decimal-only message (LKF-313).
- A lambda out parameter was taken as assigned on entry (LKF-316).
- A match whose arms mix a fresh `new` and an owned-returning call lost the owner and could not
  initialize a raii local (LKF-326).
- A pinned local was rejected in any async function with a suspension, even when its block ended
  before it (LKF-321).

Other boundaries keep their diagnostic and are checked here with a control: `??=` on a property or
an indexer, owners in an expanded params span (now with a specific message, LKF-282), move-only
tuple deconstruction (LKF-278), view reassignment with a shorter-lived owner (LKF-279), FnOnce
through a parameter (LKF-287) and the owned argument named out of order (LKF-288).
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

MAIN = 'class Program { public static int Main() { BODY } }'
CORE = 'using NativeSharp.Core;\n'
ATOMIC = 'using NativeSharp.Core.Threading;\n'
PAYLOAD = ('class Payload { public int Value; public Payload(int v) { Value = v; } }\n')
VIEW = ('public borrowed struct View<lifetime a> { private borrowed[a] int* target; '
        'public View(ref[a] int value) { unsafe("view") { target = &value; } } '
        'public int Get() { unsafe("view") { return *target; } } }\n'
        'struct Owner { public int Value; ~Owner() { } }\n')
SPAN = 'borrowed struct ReadOnlySpan<T> { public T* start; public int count; }\n'
REST = 'int r = 0; match (a) { [first, ..rest] => { r = first + rest.Length(); }, _ => { r = 1; } } '
MOVE_ONLY = 'struct Res { public int V; public Res(int v) { V = v; } ~Res() { } }\n'
DROPS = ('class Payload { public static int Drops = 0; public int Value; public Payload(int v) { Value = v; } '
         '~Payload() { Drops = Drops + 1; } }\n')
FAIL = 'static owned Payload Fail() throws Exception { throw new Exception(); } '
NS3 = ('namespace A { public class T { public static int V() { return 1; } } }\n'
       'namespace B { public class T { public static int V() { return 2; } } }\n'
       'namespace C { public class T { public static int V() { return 3; } } }\n')

EXECUTOR = ('using NativeSharp.Core;\nstruct InlineExecutor : IExecutor { public int spawned; '
            'public InlineExecutor() { spawned = 0; } public int Spawn(Future<void> task) { spawned = spawned + 1; '
            'return spawned - 1; } public int PollJoin(int handle) { return 0; } '
            'public void Cancel(int handle) { } }\n')
CHILD = 'class Program { static async void Child(int a, ref int v) { v = v + a; } public static int Main() { '
SCOPES = ('TaskScope<InlineExecutor> s1 = TaskScope<InlineExecutor>.Create(new InlineExecutor()); '
          'TaskScope<InlineExecutor> s2 = TaskScope<InlineExecutor>.Create(new InlineExecutor()); ')
BUILDER = ('struct MyList { public int Count; public static MyList CreateBuilder() { return new MyList(); } ADDS'
           'public MyList ToCollection() { return this; } }\n')
ADD_FLOAT = 'public void Add(float x) { Count = Count + 10; } '
ADD_INT = 'public void Add(int x) { Count = Count + 21; } '

CLASSES = ('class B { public virtual int V() { return 1; } }\n'
           'class D1 : B { public override int V() { return 42; } }\n'
           'class D2 : B { public override int V() { return 2; } }\n')
PIN_BOX = 'class Box { public int Id; public Box(int i) { Id = i; } }\n'

CASES = {
    # LKF-277
    'coalescing a string array element then delete[]': (False, MAIN.replace('BODY',
        'string[] a = new string[2]; a[1] ??= "ab"; int n = a[1].Length; delete[] a; return n + 40;'), 42),
    'assigning a string array element then delete[]': (False, MAIN.replace('BODY',
        'string[] a = new string[2]; a[1] = "ab"; int n = a[1].Length; delete[] a; return n + 40;'), 42),
    'coalescing a field': (False, 'class Box { public string Name; }\n' + MAIN.replace('BODY',
        'raii Box b = new Box(); b.Name ??= "abc"; b.Name ??= "zz"; return b.Name.Length + 39;'), 42),
    'coalescing a property': (False, 'class Box { private string name; public string Name { get { return name; } '
        'set { name = value; } } }\n' + MAIN.replace('BODY', 'raii Box b = new Box(); b.Name ??= "abc"; return 42;'),
        "left operand of '?' '?=' must be addressable storage"),
    'coalescing an indexer': (False, 'class Box { private string name; '
        'public string this[int i] { get { return name; } set { name = value; } } }\n' +
        MAIN.replace('BODY', 'raii Box b = new Box(); b[0] ??= "abc"; return 42;'),
        "left operand of '?' '?=' must be addressable storage"),
    'coalescing a cached property value': (False, 'class Box { private string name; public string Name { get { '
        'return name; } set { name = value; } } }\n' + MAIN.replace('BODY',
        'raii Box b = new Box(); string n = b.Name; n ??= "abc"; b.Name = n; return b.Name.Length + 39;'), 42),
    # LKF-278
    'deconstructing a move-only tuple': (False, MOVE_ONLY + 'class Program { static (Res, int) Make() { '
        'return (new Res(40), 2); } public static int Main() { var (a, b) = Make(); return a.V + b; } }',
        'move-only elements cannot be split out of a tuple'),
    'deconstructing a trivial tuple': (False, 'class Program { static (int, int) Make() { return (40, 2); } '
        'public static int Main() { var (a, b) = Make(); return a + b; } }', 42),
    # LKF-279
    'view reassigned to a shorter-lived owner': (False, VIEW + MAIN.replace('BODY',
        'Owner a; a.Value = 42; bool c = true; View v = new View(ref a.Value); '
        '{ Owner b; b.Value = 7; if (c) { v = new View(ref b.Value); } } return v.Get();'),
        "view borrowed from local 'b' cannot be stored in a longer-lived local"),
    'view of one owner': (False, VIEW + MAIN.replace('BODY',
        'Owner a; a.Value = 42; View v = new View(ref a.Value); return v.Get();'), 42),
    # LKF-280
    'conditional member assignment of a string to an int': (False, 'class Box { public int Value; }\n' +
        MAIN.replace('BODY', 'Box p = new Box(); p?.Value = "x"; return p.Value;'),
        "cannot implicitly convert assignment RHS from 'string-literal' to 'builtin:int'"),
    'conditional member assignment narrowing a long': (False, 'class Box { public int Value; }\n' +
        MAIN.replace('BODY', 'Box p = new Box(); long l = 5; p?.Value = l; return p.Value;'),
        "cannot implicitly convert assignment RHS from 'builtin:long' to 'builtin:int'"),
    'conditional member assignment': (False, 'class Box { public int Value; }\n' +
        MAIN.replace('BODY', 'Box p = new Box(); p?.Value = 42; return p.Value;'), 42),
    'conditional member assignment widening an int': (False, 'class Box { public long Value; }\n' +
        MAIN.replace('BODY', 'Box p = new Box(); int i = 42; p?.Value = i; return (int)p.Value;'), 42),
    'conditional member assignment on null': (False, 'class Box { public int Value; }\n' +
        MAIN.replace('BODY', 'Box p = null; p?.Value = 7; return 42;'), 42),
    # LKF-281
    'three same-named types in unimported namespaces': (False, NS3 +
        'namespace D { class Program { public static int Main() { return T.V(); } } }',
        "unknown identifier 'T'"),
    'three same-named types in imported namespaces': (False, 'using A; using B; using C;\n' + NS3 +
        'namespace D { class Program { public static int Main() { return T.V(); } } }',
        "ambiguous type name 'T'"),
    'same-named type in the current namespace': (False, NS3 +
        'namespace D { public class T { public static int V() { return 42; } } '
        'class Program { public static int Main() { return T.V(); } } }', 42),
    # LKF-282
    'owner in an expanded params span': (False, SPAN + 'class Resource { }\nclass Program { '
        'private static int Sum(params ReadOnlySpan<Resource> values) { return values.count; } '
        'public static int Main() { owned Resource a = new Resource(); return Sum(move(a)); } }',
        'an expanded params span only borrows its elements for the call'),
    'values in an expanded params span': (False, SPAN + 'class Program { '
        'private static int Sum(params ReadOnlySpan<int> values) { return values.count; } '
        'public static int Main() { return Sum(1, 2) + 40; } }', 42),
    # LKF-283
    'named atomic compare-exchange': (True, ATOMIC + MAIN.replace('BODY',
        'raii AtomicInt32 a = new AtomicInt32(0); int old = a.CompareExchange(comparand: 0, value: 42, '
        'failureOrder: MemoryOrder.Relaxed, successOrder: MemoryOrder.Acquire); return a.Value + old;'),
        'atomic operations take positional arguments'),
    'named atomic store': (True, ATOMIC + MAIN.replace('BODY',
        'raii AtomicInt32 a = new AtomicInt32(0); a.Store(order: MemoryOrder.Release, value: 42); return 42;'),
        'atomic operations take positional arguments'),
    'positional atomic compare-exchange': (True, ATOMIC + MAIN.replace('BODY',
        'raii AtomicInt32 a = new AtomicInt32(0); '
        'int old = a.CompareExchange(42, 0, MemoryOrder.Acquire, MemoryOrder.Relaxed); return a.Value + old;'), 42),
    # LKF-284
    'array rest capture': (True, CORE + MAIN.replace('BODY',
        'int[] a = new int[3]; a[0] = 40; a[1] = 1; a[2] = 1; ' + REST + 'delete[] a; return r;'), 42),
    'fixed array rest capture': (True, CORE + MAIN.replace('BODY',
        'int[3] a; a[0] = 40; a[1] = 1; a[2] = 1; ' + REST + 'return r;'), 42),
    'array rest capture without Core': (False, MAIN.replace('BODY',
        'int[] a = new int[3]; a[0] = 40; ' + REST + 'delete[] a; return r;'),
        'reference NativeSharp.Core, which declares the views a rest binding uses'),
    'string rest capture': (True, CORE + MAIN.replace('BODY',
        'string a = "xyz"; int r = 0; match (a) { [first, ..rest] => { r = 40 + rest.Length(); }, '
        '_ => { r = 1; } } return r;'), 42),
    # NSC-NEW-018: a fresh array passed for the first time while an older call-result view is alive.
    'fresh array passed while a call-result view is alive': (True, CORE + MAIN.replace('BODY',
        'int[] nums = new int[4]; Span<int> t = Spans.OfRange<int>(nums, 0, 4); t[0] = 40; '
        'int[] other = new int[4]; int copied = t.CopyTo(Spans.OfRange<int>(other, 0, 4)); '
        'int r = other[0] + copied - 2; delete[] other; delete[] nums; return r;'), 42),
    # LKF-289
    'TaskScope joined before the borrowed local ends': (True, EXECUTOR + CHILD + SCOPES +
        '{ int x = 1; s1.Spawn(Child(1, ref x)); s1.Join(); } return 42; } }', 42),
    'TaskScope child outliving its borrowed local': (True, EXECUTOR + CHILD + SCOPES +
        '{ int x = 1; s1.Spawn(Child(1, ref x)); } s1.Join(); return 42; } }', 'scoped child borrow outlives'),
    'another TaskScope joined': (True, EXECUTOR + CHILD + SCOPES +
        '{ int x = 1; s1.Spawn(Child(1, ref x)); s2.Join(); } s1.Join(); return 42; } }',
        'scoped child borrow outlives'),
    'unrelated Join method': (True, EXECUTOR + 'class J { public void Join() { } }\n' + CHILD + SCOPES +
        'J j = new J(); { int x = 1; s1.Spawn(Child(1, ref x)); j.Join(); } s1.Join(); return 42; } }',
        'scoped child borrow outlives'),
    'child call with named arguments': (True, EXECUTOR + CHILD + SCOPES +
        '{ int x = 1; s1.Spawn(Child(v: ref x, a: 1)); } s1.Join(); return 42; } }', 'scoped child borrow outlives'),
    'spawned Future local': (True, EXECUTOR + CHILD + SCOPES +
        '{ int x = 1; Future<void> f = Child(1, ref x); s1.Spawn(move(f)); } s1.Join(); return 42; } }',
        'scoped child borrow outlives'),
    # LKF-297
    'null-conditional user indexer': (False, 'class Box { private int v = 42; public int this[int i] { '
        'get { return v; } set { v = value; } } }\n' + MAIN.replace('BODY',
        'Box b = new Box(); var x = b?[0]; return 42;'), 42),
    # LKF-298
    'collection builder with Add(float) declared first': (False, BUILDER.replace('ADDS', ADD_FLOAT + ADD_INT) +
        MAIN.replace('BODY', 'MyList l = [1, 1]; return l.Count;'), 42),
    'collection builder with Add(int) declared first': (False, BUILDER.replace('ADDS', ADD_INT + ADD_FLOAT) +
        MAIN.replace('BODY', 'MyList l = [1, 1]; return l.Count;'), 42),
    'collection builder with mixed elements': (False, BUILDER.replace('ADDS', ADD_INT + ADD_FLOAT) +
        MAIN.replace('BODY', 'MyList l = [1, 2.5f]; return l.Count + 22;'), 42),
    'empty collection builder': (False, BUILDER.replace('ADDS', ADD_INT + ADD_FLOAT) +
        MAIN.replace('BODY', 'MyList l = []; return l.Count + 42;'), 42),
    'collection builder with equally good Add overloads': (False, BUILDER.replace('ADDS',
        'public void Add(long x) { Count = Count + 1; } public void Add(double x) { Count = Count + 2; } ') +
        MAIN.replace('BODY', 'MyList l = [1, 2]; return l.Count + 40;'),
        'collection expression elements match more than one builder Add equally'),
    # LKF-293
    'large fixed-size array local': (False, MAIN.replace('BODY',
        'int[100000] a; a[99999] = 42; return a[99999];'), 42),
    'large struct local': (False, 'struct S { public int[100000] Items; }\n' +
        MAIN.replace('BODY', 'S s; s.Items[99999] = 42; return s.Items[99999];'), 42),
    # LKF-302
    'stackalloc of a struct with a nested owned field': (False, 'struct Inner { public owned string Text; }\n'
        'struct Outer { public Inner I; }\n' + MAIN.replace('BODY',
        'unsafe("t") { Outer* slots = stackalloc Outer[2]; } return 42;'),
        "'type:P::Outer' owns a lifetime that raw storage cannot discharge"),
    'stackalloc of a struct with a nested destructor': (False, 'struct Inner { public int V; ~Inner() { } }\n'
        'struct Outer { public Inner I; }\n' + MAIN.replace('BODY',
        'unsafe("t") { Outer* slots = stackalloc Outer[2]; } return 42;'),
        "'type:P::Outer' owns a lifetime that raw storage cannot discharge"),
    'stackalloc of a plain nested struct': (False,
        'struct Inner { public int V; }\nstruct Outer { public Inner I; }\n' +
        MAIN.replace('BODY', 'unsafe("t") { Outer* slots = stackalloc Outer[2]; slots[1].I.V = 42; '
        'return slots[1].I.V; }'), 42),
    # LKF-312
    'conditional joining a class and its base': (False, CLASSES + MAIN.replace('BODY',
        'bool c = true; B b = c ? (B)new D1() : new D2(); int v = b.V(); delete b; return v;'), 42),
    'conditional over two locals of related classes': (False, CLASSES + MAIN.replace('BODY',
        'bool c = false; raii B a = new B(); raii D2 d = new D2(); return (c ? a : d).V() + 40;'), 42),
    'conditional over two sibling classes': (False, CLASSES + MAIN.replace('BODY',
        'bool c = true; B b = c ? new D1() : new D2(); int v = b.V(); delete b; return v;'),
        'the branches of a conditional expression have incompatible types'),
    # LKF-313
    'wrapping integer arithmetic': (False, MAIN.replace('BODY',
        'int m = 2147483647; int r = wrapping(m + 1); return r == -2147483647 - 1 ? 42 : 1;'), 42),
    'wrapping decimal arithmetic': (False, MAIN.replace('BODY',
        'decimal d = 1m; int r = wrapping((int)(d + 1m)); return 42;'),
        "overflow policy 'wrapping' is not allowed for decimal arithmetic"),
    # LKF-316
    'lambda out parameter never assigned': (False, 'delegate void D(out int x);\n' + MAIN.replace('BODY',
        'D d = (out int x) => { }; int v; d(out v); return 42;'), "out parameter 'x' of the lambda is not assigned"),
    'lambda out parameter assigned on one branch': (False, 'delegate void D(bool c, out int x);\n' +
        MAIN.replace('BODY', 'D d = (bool c, out int x) => { if (c) { x = 42; } }; int v; d(true, out v); return v;'),
        "out parameter 'x' of the lambda is not assigned"),
    'lambda out parameter skipped by an early return': (False, 'delegate void D(bool c, out int x);\n' +
        MAIN.replace('BODY', 'D d = (bool c, out int x) => { if (c) { return; } x = 42; }; int v; '
        'd(false, out v); return v;'), "out parameter 'x' of the lambda is not assigned"),
    'lambda out parameter assigned on both branches': (False, 'delegate void D(bool c, out int x);\n' +
        MAIN.replace('BODY', 'D d = (bool c, out int x) => { if (c) { x = 42; } else { x = 1; } }; int v; '
        'd(true, out v); return v;'), 42),
    'expression lambda assigning its out parameter': (False, 'delegate int D(out int x);\n' + MAIN.replace('BODY',
        'D d = (out int x) => x = 42; int v; int r = d(out v); return v;'), 42),
    # LKF-321
    'pinned local whose block ends before a suspension': (False, PIN_BOX + 'class Program { public static async int '
        'Run() { { pin raii Box b = new Box(42); int v = b.Id; } yield; return 42; } '
        'public static int Main() { return 42; } }', 42),
    'pinned local alive across a nested suspension': (False, PIN_BOX + 'class Program { public static async int '
        'Run() { pin raii Box b = new Box(42); { yield; } return b.Id; } public static int Main() { return 42; } }',
        'cannot hold a pinned local across a suspension'),
    # LKF-287
    'FnOnce invoked twice through a parameter': (False, 'delegate int Read();\nclass Box { public int Value; '
        'public Box(int v) { Value = v; } }\nclass Program { static int Twice(Read r) { return r() + r(); } '
        'public static int Main() { raii Box box = new Box(21); Read action = [move box] () => box.Value; '
        'int v = Twice(action); delete action; return v; } }',
        'FnOnce closure may be invoked more than once through a parameter'),
    # LKF-288
    'owned argument named out of order without move': (False, PAYLOAD + 'class Holder { public owned Payload Item; '
        'public int Count; public Holder(owned Payload item, int count) { Item = move(item); Count = count; } }\n' +
        MAIN.replace('BODY', 'owned Payload p = new Payload(1); raii Holder h = new Holder(count: 41, item: p); '
        'return h.Count + h.Item.Value;'), "owned parameter 'item'"),
    'owned argument named out of order': (False, PAYLOAD + 'class Holder { public owned Payload Item; '
        'public int Count; public Holder(owned Payload item, int count) { Item = move(item); Count = count; } }\n' +
        MAIN.replace('BODY', 'owned Payload p = new Payload(1); raii Holder h = new Holder(count: 41, item: move(p)); '
        'return h.Count + h.Item.Value;'), 42),
    # LKF-326
    'match arms mixing a throwing owned call and new': (True, CORE + DROPS + 'class Program { ' + FAIL +
        'public static int Main() { int k = 1; int v = 0; try { raii Payload r = match (k) { 0 => Fail(), '
        '_ => new Payload(42) }; v = r.Value; } catch (Exception e) { } return v + 1 - Payload.Drops; } }', 42),
    'match arm that throws before new': (True, CORE + DROPS + 'class Program { ' + FAIL +
        'public static int Main() { int k = 0; int v = 0; try { raii Payload r = match (k) { 0 => Fail(), '
        '_ => new Payload(7) }; v = r.Value; } catch (Exception e) { v = 42; } return v - Payload.Drops; } }', 42),
    'match value arm after a throwing first arm': (True, CORE + 'class Program { static int Fail() throws Exception '
        '{ throw new Exception(); } public static int Main() { int k = 1; int r = 0; try { r = match (k) { '
        '0 => Fail(), _ => 42 }; } catch (Exception e) { } return r; } }', 42),
    'match borrowed arm after a throwing first arm': (True, CORE + DROPS + 'class Program { ' + FAIL +
        'public static int Main() { raii Payload keep = new Payload(42); int k = 1; int v = 0; try { '
        'Payload r = match (k) { 0 => Fail(), _ => keep }; v = r.Value; } catch (Exception e) { } return v; } }', 42),
    # LKF-291
    'null-conditional call with an Optional result': (True, CORE + 'class Box { public Optional<int> Get() { '
        'return new Optional<int>(42); } }\n' + MAIN.replace('BODY',
        'Box p = new Box(); Optional<int> x = p?.Get(); return x ?? 1;'), 42),
    'skipped null-conditional call with an Optional result': (True, CORE + 'class Box { public Optional<int> Get() '
        '{ return new Optional<int>(7); } }\n' + MAIN.replace('BODY',
        'Box p = null; Optional<int> x = p?.Get(); return x ?? 42;'), 42),
    'null-conditional call with an int result': (False, 'class Box { public int Get() { return 42; } }\n' +
        MAIN.replace('BODY', 'Box p = new Box(); var x = p?.Get(); return 42;'),
        'a null-conditional method must return a nullable pointer/reference carrier'),
}


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=180)


def build_core(compiler, root, library_sources):
    libraries = root / 'libraries'
    libraries.mkdir()
    shutil.copytree(library_sources / 'NativeSharp.Core', libraries / 'core', ignore=shutil.ignore_patterns('obj', 'bin'))
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
    library_sources = Path(sys.argv[2]).resolve() / 'stdlib'
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-assignment-call-') as temporary:
        root = Path(temporary)
        core = build_core(compiler, root, library_sources)
        for index, (name, (needs_library, source, expected)) in enumerate(CASES.items()):
            folder = root / ('case-' + str(index))
            folder.mkdir()
            (folder / 'Program.ns').write_text(source, encoding='utf-8')
            atomic = source.startswith(ATOMIC)
            if atomic:
                shutil.copyfile(library_sources / 'NativeSharp.Core.Threading/Atomics.ns', folder / 'Atomics.ns')
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
