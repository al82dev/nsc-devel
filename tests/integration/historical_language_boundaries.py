"""Historical language boundaries verified in this revision (LKF-049/053/056/062/063/065/071/077/078/080/082).

Each program either exits 42 at O0 and O2 or is rejected with the exact diagnostic, and each rejection
has an accepted control that differs only in the property checked:

- static_assert folds const fields (transitively), enum constants and default(T) (LKF-053); a field
  that is not const and a false condition are still rejected;
- overloads that differ only in owned/borrowed get distinct symbols (LKF-062, linked twice before);
- delete[] is rejected while a ref local to one of its elements is in scope (LKF-063);
- a struct with an owned field is moved, not copied (LKF-065);
- delete this is rejected outside a destructor (LKF-071);
- the documented boundaries that remain have explicit diagnostics: partial struct and partial
  generic types (LKF-049), raw unions with owned fields (LKF-078), generic export extern C (LKF-080),
  a constraint on a global generic function, a struct implementing an interface and `where T : U`
  (LKF-082); user ++/--, generic methods in generic types and `where T : struct` run (LKF-077/080/082);
- a constant shift count out of range traps at run time like a runtime one (131), and a compound
  property assignment needs a re-evaluable receiver (LKF-056).
"""
from pathlib import Path
import subprocess
import sys
import tempfile

CONSTANTS = ('enum Color { Red = 40, Green = 41 }\nenum Wide : ulong { Top = 4294967295 }\n'
             'class K { public const int Two = 2; public const int Four = Two * 2; public static int NotConst = 3; }\n')


def program(body):
    return CONSTANTS + 'class Program { public static int Main() { ' + body + ' return 42; } }'


CASES = {
    'static_assert on a const field': (program('static_assert(K.Two == 2, "x");'), 42),
    'static_assert on a const field chain': (program('static_assert(K.Four == 4, "x");'), 42),
    'static_assert on an enum cast': (program('static_assert((int)Color.Red == 40, "x");'), 42),
    'static_assert comparing enum constants': (program('static_assert(Color.Red != Color.Green, "x");'), 42),
    'static_assert on a wide enum': (program('static_assert((ulong)Wide.Top == 4294967295ul, "x");'), 42),
    'static_assert on default values': (program('static_assert(default(int) == 0 && !default(bool), "x");'), 42),
    'static_assert false on a const field': (program('static_assert(K.Two == 3, "two");'),
                                             'static assertion failed: two'),
    'static_assert on a field that is not const': (program('static_assert(K.NotConst == 3, "x");'),
                                                   'static_assert condition is not a compile-time bool constant'),
    'owned and borrowed overloads': ('''class Item { public int V; }
class Program { static int F(owned Item i) { int v = i.V; delete i; return v; }
 static int F(borrowed Item i) { return i.V + 1; }
 public static int Main() { raii Item a = new Item(); a.V = 40; int borrowedResult = F(a);
  Item b = new Item(); b.V = 1; int ownedResult = F(move(b)); return borrowedResult + ownedResult; } }''', 42),
    'delete[] while an element ref is alive': ('''class Program { public static int Main() {
 int[] a = new int[2]; a[0] = 42; ref int e = ref a[0]; delete[] a; return e; } }''',
        "cannot delete[] array 'a' while reference 'e' to one of its elements is alive"),
    'delete[] of an owned parameter while an element ref is alive': ('''class Program {
 static int F(owned int[] a) { ref int e = ref a[1]; delete[] a; return e; } public static int Main() {
 return 42; } }''',
                                                                     "cannot delete[] array 'a' while reference 'e'"),
    'delete[] after the element ref scope ended': ('''class Program { public static int Main() {
 int[] a = new int[2]; int r = 0; { ref int e = ref a[0]; e = 42; r = e; } delete[] a; return r; } }''', 42),
    'copy of a struct with an owned field': ('''class Item { } struct S { public owned Item item; }
class Program { public static int Main() { S a; a.item = new Item(); S b = a; return 42; } }''',
        "move-only value of type 'type:Boundaries::S' cannot be copied into local 'b'"),
    'move of a struct with an owned field': ('''class Item { public int V; } struct S { public owned Item item; }
class Program { public static int Main() { S a; a.item = new Item(); a.item.V = 42; S b = move(a); return
 b.item.V; } }''',
                                             42),
    'delete this in an ordinary method': ('''class Node { public void Kill() { delete this; } }
class Program { public static int Main() { return 42; } }''',
        'delete this is forbidden; the method borrows its receiver'),
    'delete of an owned parameter in a method': ('''class Node { public void Kill(owned Node other) { delete other; } }
class Program { public static int Main() { raii Node n = new Node(); n.Kill(new Node()); return 42; } }''', 42),
    'partial struct': ('''partial struct Pair { public int A; } partial struct Pair { public int B; }
class Program { public static int Main() { return 42; } }''', "'partial' is supported only for class declarations"),
    'partial generic class': ('''partial class Box<T> { public T Value; } partial class Box<T> { public T
 Get() { return Value; } }
class Program { public static int Main() { return 42; } }''', 'partial generic types are deferred beyond Stage 44'),
    'partial class': ('''partial class Box { public int Value; } partial class Box { public int Get() {
 return Value; } }
class Program { public static int Main() { Box b = new Box(); b.Value = 42; return b.Get(); } }''', 42),
    'raw union with an owned field': ('''class Item { } union U { public owned Item a; public int b; }
class Program { public static int Main() { return 42; } }''', "owned field 'a' is not permitted in a raw union"),
    'raw union of values': ('''union U { public int a; public float b; }
class Program { public static int Main() { U u; u.a = 42; return u.a; } }''', 42),
    'generic export extern C': ('''class Api { public static export extern C T Id<T>(T value) { return value; } }
class Program { public static int Main() { return 42; } }''',
                                'export extern C does not expose generic methods or members of generic types'),
    'generic method in a generic type': ('''class Box<T> { public T Value; public U Map<U>(U other) { return other; } }
class Program { public static int Main() { Box<int> b = new Box<int>(); return b.Map<int>(42); } }''', 42),
    'constraint on a global generic function': ('''interface IValue { int Get(); }
T First<T>(T value) where T : IValue { return value; }
int Main() { return 42; }''', 'a generic constraint on a global function is not supported yet'),
    'struct implementing an interface': ('''interface IValue { int Get(); }
struct S : IValue { public int X; public int Get() { return X; } }
class Program { public static int Main() { return 42; } }''', 'a NativeSharp 3 struct may implement traits only'),
    'type parameter as a constraint': ('''class Program { static U Up<T, U>(T value) where T : U { return value; }
 public static int Main() { return 42; } }''',
        "'U' is a generic type parameter of this declaration and cannot be used as a constraint"),
    'struct constraint': ('''class Program { static T Zero<T>() where T : struct { return default(T); }
 public static int Main() { return Zero<int>() + 42; } }''', 42),
    'struct constraint with a class argument': ('''class Item { } class Program {
 static T Zero<T>() where T : struct { return default(T); } public static int Main() { Item i = Zero<Item>();
 return 42; } }''',
                                                "requires a value type ('where T : struct')"),
    'user increment and decrement': ('''struct Counter { public int V;
 public static Counter operator ++(Counter c) { Counter r; r.V = c.V + 1; return r; }
 public static Counter operator --(Counter c) { Counter r; r.V = c.V - 1; return r; } }
class Program { public static int Main() { Counter c; c.V = 41; c++; c++; c--; Counter d = c++; return c.V +
 d.V - 43; } }''',
                                     42),
    'constant shift count out of range': ('class Program { public static int Main() { int x = 1 << 40; '
                                          'return x; } }', 131),
    'runtime shift count out of range': ('''class Program { static int N() { return 40; }
 public static int Main() { int x = 1; return x << N(); } }''', 131),
    'shift count in range': ('''class Program { static int N() { return 1; }
 public static int Main() { int x = 21; return x << N(); } }''', 42),
    'compound property assignment through a call receiver': ('''class Box { public int Value { get; set; } }
class Program { static Box Shared = new Box(); static Box Get() { return Shared; }
 public static int Main() { Shared.Value = 40; Get().Value += 2; return Shared.Value; } }''',
                                                             'assign this one to a local first and use that local'),
    'compound property assignment through a local receiver': ('''class Box { public int Value { get; set; } }
class Program { public static int Main() { Box b = new Box(); b.Value = 40; b.Value += 2; return b.Value; } }''', 42),
}

PROJECT = ('<Project><PropertyGroup><Name>Boundaries</Name><OutputType>Exe</OutputType>'
           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" /></ItemGroup></Project>')


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=120)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-historical-boundaries-') as temporary:
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
                if result.returncode == 0 or expected not in output:
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
    print('historical_language_boundaries: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
