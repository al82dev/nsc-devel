"""Generic inference, aliases, delegate targets, captures, events and array ownership boundaries.

Fichas LKF-083, 085, 086, 087, 088, 090, 092, 093, 096, 097 and 098. Each program either exits with
the expected code at O0 and O2 or is rejected with the exact diagnostic, and every rejection has an
accepted control. Two defects were fixed with these cases: a spread of a null array read address
zero (now traps 135, LKF-096) and `values[i] = values[i] + 1` on a variable-length array was
rejected as a pointer escape (LKF-097). Event handlers run in subscription order (FIFO) like C#
multicast delegates; the historical chapter that describes LIFO is out of date (LKF-088).
"""
from pathlib import Path
import subprocess
import sys
import tempfile

CASES = {
    'incompatible evidence for one type parameter': ('''class Program { static T Pick<T>(T a, T b) { return a; }
 public static int Main() { double d = Pick(1, 2.5); return (int)d + 41; } }''',
        "type arguments for generic method 'Pick' cannot be inferred"),
    'no evidence and no target type': ('''class Program { static T Make<T>() { return default(T); } public static
 int Main() { Make(); return 42; } }''',
        "type arguments for generic method 'Make' cannot be inferred"),
    'inference from the target type': ('''class Program { static T Make<T>() { return default(T); } public static
 int Main() { int v = Make(); return v + 42; } }''',
        42),
    'null argument typed by the target': ('''class Item { } class Program { static T Identity<T>(T value) { return
 value; } public static int Main() { Item v = Identity(null); return v == null ? 42 : 1; } }''',
        42),
    'lambda argument typed by the target': ('''delegate int Transformer(int value); class Program { static T
 Identity<T>(T value) { return value; } public static int Main() { Transformer f = Identity(x => x + 1); return
 f(41); } }''',
        42),
    'generic alias of a constrained type': ('''class Box<T> where T : struct { public T Value; }
using Boxed<T> = Box<T>;
class Program { public static int Main() { Boxed<int> b = new Boxed<int>(); b.Value = 42; return b.Value; } }''',
        42),
    'alias applies the underlying constraint': ('''class Item { } class Box<T> where T : struct { public T Value; }
using Boxed<T> = Box<T>;
class Program { public static int Main() { Boxed<Item> b = null; return 42; } }''',
        "requires a value type ('where T : struct')"),
    'generic alias with its own where clause': ('''class Box<T> { public T Value; }
using Boxed<T> = Box<T> where T : struct;
class Program { public static int Main() { return 42; } }''',
        "expected ';', found 'where'"),
    'virtual method as delegate target dispatches the override': ('''delegate int Transform(int value); class
 Worker { public virtual int Apply(int value) { return value; } }
class Better : Worker { public override int Apply(int value) { return value + 1; } }
class Program { public static int Main() { Worker w = new Better(); Transform t = w.Apply; return t(41); } }''',
        42),
    'struct receiver as delegate target': ('''delegate int Transform(int value); struct S { public int B; public
 int Apply(int value) { return value + B; } }
class Program { public static int Main() { S s; s.B = 1; Transform t = s.Apply; return t(41); } }''',
        'delegate instance targets must be class references'),
    'generic parameter as delegate return type': ('''delegate T Producer<T>(); class Program { static int Make() {
 return 42; } public static int Main() { Producer<int> p = Make; return p(); } }''',
        42),
    'lambda capturing a raii local': ('''delegate int F(); class Item { public int V = 42; }
class Program { public static int Main() { raii Item i = new Item(); F f = () => i.V; return f(); } }''',
        'cannot be captured by a lambda without an explicit move capture'),
    'lambda capturing an owned parameter': ('''delegate int F(); class Item { public int V = 42; }
class Program { static F Make(owned Item i) { return () => i.V; } public static int Main() { return 42; } }''',
        'cannot be captured by a lambda without an explicit move capture'),
    'lambda capturing a variable-length array': ('''delegate int F(); class Program { static int Use(int n) {
 int[n] v; v[0] = 42; F f = () => v[0]; return f(); } public static int Main() { return Use(2); } }''',
        'cannot be captured by a lambda because the delegate may outlive its stack frame'),
    'explicit move capture': ('''delegate int F(); class Item { public int V = 42; }
class Program { public static int Main() { Item i = new Item(); F f = [move i] () => i.V; return f(); } }''',
        42),
    'value capture': ('''delegate int F(); class Program { public static int Main() { int x = 42; F f = () => x;
 return f(); } }''',
        42),
    'event handlers run in subscription order': ('''delegate void Changed(int value);
class Publisher { public event Changed Event; public void Raise(int value) { Event(value); } }
class Program { static int Trace; static void First(int v) { Trace = Trace * 10 + 1; } static void Second(int v) {
 Trace = Trace * 10 + 2; }
 public static int Main() { Publisher p = new Publisher(); p.Event += First; p.Event += Second; p.Raise(0); return
 Trace; } }''',
        12),
    'delete[] of an owned array destroys its elements': ('''class Item { public static int Drops; ~Item() { Drops =
 Drops + 1; } }
class Program { public static int Main() { { owned Item[] a = new Item[2]; a[0] = new Item(); a[1] = new Item();
 delete[] a; } return Item.Drops == 2 ? 42 : Item.Drops; } }''',
        42),
    'delete[] of a plain array leaves its elements': ('''class Item { public static int Drops; ~Item() { Drops =
 Drops + 1; } }
class Program { static Item Keep = new Item(); public static int Main() { { Item[] a = new Item[1]; a[0] = Keep;
 delete[] a; } return Item.Drops == 0 ? 42 : Item.Drops; } }''',
        42),
    'moving an array owner while a borrow is alive': ('''class Program { public static int Main() { raii int[] a =
 new int[2]; borrowed int[] v = a; raii int[] b = move(a); return v[0]; } }''',
        'cannot move owner'),
    'moving an array owner after the borrow ended': ('''class Program { public static int Main() { raii int[] a =
 new int[2]; a[0] = 42; int r = 0; { borrowed int[] v = a; r = v[0]; } raii int[] b = move(a); return r; } }''',
        42),
    'inline array parameter by value': ('''class Program { static int Sum(int[4] values) { return values[0]; }
 public static int Main() { return 42; } }''',
        'an inline array cannot be passed by value'),
    'inline array returned by value': ('''class Program { static int[4] Make() { int[4] v; return v; } public
 static int Main() { return 42; } }''',
        'an inline array cannot be returned by value'),
    'inline array inside a struct passed by ref': ('''struct H { public int[4] Values; } class Program { static int
 Sum(ref H h) { return h.Values[0] + h.Values[3]; } public static int Main() { H h; h.Values[0] = 40; h.Values[3] =
 2; return Sum(ref h); } }''',
        42),
    'spread of a null array': ('''class Program { static int[] Get() { return null; } public static int Main() {
 int[] a = Get(); int[] b = [..a, 1]; return b.Length; } }''',
        135),
    'spread of an array': ('''class Program { public static int Main() { int[] a = new int[2]; a[1] = 41; int[] b =
 [..a, 1]; return b[1] + b[2]; } }''',
        42),
    'variable-length array element updated from itself': ('''class Program { static int Use(int n) { int[n] values;
 values[0] = 41; values[0] = values[0] + 1; return values[0]; } public static int Main() { return Use(2); } }''',
        42),
    'pointer into a variable-length array stored in a static': ('''class Program { static int* Kept; static int
 Use(int n) { int[n] values; Kept = &values[0]; return 0; } public static int Main() { return Use(2); } }''',
        'cannot be stored outside a local alias'),
    'flexible tail indexed through a pointer (raw tier)': ('''struct Packet { public int Count; public int[*] Values; }
class Program { public static int Main() { long* words = stackalloc long[4]; Packet* p = (Packet*)words; p->Count =
 2; p->Values[1] = 42; return p->Values[1]; } }''',
        42),
}

PROJECT = ('<Project><PropertyGroup><Name>P</Name><OutputType>Exe</OutputType>'
           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" /></ItemGroup></Project>')


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=120)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-generic-delegate-array-') as temporary:
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
    print('generic_delegate_array_boundaries: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
