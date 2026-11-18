"""Library comments that recorded compiler limits, contrasted with the current compiler.

Fichas LKF-176, 177, 179, 180, 184, 186, 191, 193, 195, 199, 204, 205 and 210. The Core and
Collections.Generic libraries are built from the repository first, and each program then either exits
with the expected code at O0 and O2 or is rejected with the exact diagnostic; every rejection has an
accepted control. Two defects were fixed with these cases: `delete[]` of an owned array parameter left
the parameter's exit drop in place and released the block twice (heap corruption, LKF-186), and `&s[0]`
on a string reported "bit fields have no address" (LKF-210).
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

USING = 'using NativeSharp.Core; using NativeSharp.Core.Collections.Generic;\n'
PAYLOAD = '''class Payload { public static int Drops = 0; public int Value; public Payload(int v) { Value = v; }
 ~Payload() { Drops = Drops + 1; } }
'''

# Each case: (uses the libraries, source, expected exit code or diagnostic text).
CASES = {
    'static generic call infers from the arguments': (True, USING + '''class Program { public static int Main() {
 int[] d = new int[3]; d[0] = 5; d[1] = 3; d[2] = 9; ReadOnlySpan<int> v = Spans.ReadOnlyOf<int>(d);
 return Algorithm.Find(v, 9) + 40; } }''', 42),
    'static generic call with contradictory evidence': (True, USING + '''class Program { public static int Main() {
 int[] d = new int[3]; ReadOnlySpan<int> v = Spans.ReadOnlyOf<int>(d); return Algorithm.Find(v, "x"); } }''',
        "type arguments for generic method 'Find' cannot be inferred"),
    'method group passed directly as a predicate': (True, USING + '''class Program { static bool Even(int v) {
 return v % 2 == 0; } public static int Main() { int[] d = new int[3]; d[0] = 5; d[1] = 4; d[2] = 9;
 ReadOnlySpan<int> v = Spans.ReadOnlyOf<int>(d); return Algorithm.FindIf<int>(v, Even) + 41; } }''', 42),
    'method group with inferred type arguments': (True, USING + '''class Program { static bool Even(int v) {
 return v % 2 == 0; } public static int Main() { int[] d = new int[3]; d[0] = 5; d[1] = 4; d[2] = 9;
 ReadOnlySpan<int> v = Spans.ReadOnlyOf<int>(d); return Algorithm.FindIf(v, Even) + 41; } }''', 42),
    'struct larger than eight bytes through a delegate': (False, '''struct Big { public long A; public long B;
 public long C; public long D; }
delegate Big Map(Big b, int k);
class Program { static Big Twice(Big b, int k) { Big r = b; r.A = b.A * k; r.D = b.D + k; return r; }
 public static int Main() { Map m = Twice; Big b = new Big(); b.A = 3; b.B = 7; b.C = 11; b.D = 20;
 Big r = m(b, 2); return (int)(r.A + r.B + r.C + r.D) - 4; } }''', 42),
    'struct larger than eight bytes into a capturing lambda': (False, '''struct Big { public long A; public long B;
 public long C; public long D; public long E; }
delegate long Sum(Big b);
class Program { public static int Main() { long bias = 2; Sum s = (Big b) => b.A + b.B + b.C + b.D + b.E + bias;
 Big b = new Big(); b.A = 1; b.B = 2; b.C = 3; b.D = 4; b.E = 30; return (int)s(b); } }''', 42),
    'indexer getter declaring throws': (True, USING + '''class Missing : Exception {}
class Map { private int[] keys = new int[2]; private int[] vals = new int[2];
 public Map() { keys[0] = 1; vals[0] = 40; keys[1] = 2; vals[1] = 2; }
 public int this[int key] { get throws Missing { int i = 0; while (i < 2) { if (keys[i] == key) { return vals[i]; }
 i = i + 1; } throw new Missing(); } } }
class Program { public static int Main() throws Missing { raii Map m = new Map();
 try { int x = m[7]; return 1; } catch (Missing e) { } return m[1] + m[2]; } }''', 42),
    'indexer getter throwing without declaring it': (True, USING + '''class Missing : Exception {}
class Map { public int this[int key] { get { throw new Missing(); } } }
class Program { public static int Main() { raii Map m = new Map(); return m[1]; } }''',
        "throw of 'type:P::Missing' is neither caught here nor declared"),
    'ownership transferred into an owned field': (False, PAYLOAD + '''class Node { public owned Payload Item;
 public Node(owned Payload p) { Item = move(p); } }
class Program { public static int Main() { int v = 0; { Node n = new Node(new Payload(1)); n.Item = new Payload(2);
 v = n.Item.Value; } return Payload.Drops * 10 + v + 20; } }''', 42),
    'ownership transferred through the owned List indexer': (True, USING + PAYLOAD + '''class Program {
 public static int Main() { raii List<Payload> l = new List<Payload>(); l.Add(new Payload(1));
 l[0] = new Payload(2); int v = l[0].Value; return Payload.Drops * 10 + v + 30; } }''', 42),
    'caller moves an owned array and the callee deletes it': (False, PAYLOAD + '''class Sink {
 public static int Take(owned Payload[] a) { int n = a.Length; delete[] a; return n; } }
class Program { public static int Main() { owned Payload[] a = new Payload[2]; a[0] = new Payload(1);
 a[1] = new Payload(2); int n = Sink.Take(move(a)); return Payload.Drops * 10 + n + 20; } }''', 42),
    'callee deletes an owned scalar array': (False, '''class Sink { public static int Take(owned int[] a) {
 int n = a.Length; delete[] a; return n; } }
class Program { public static int Main() { owned int[] a = new int[2]; return Sink.Take(move(a)) + 40; } }''', 42),
    'fresh array passed to an owned array parameter': (False, PAYLOAD + '''class Sink {
 public static int Take(owned Payload[] a) { int n = a.Length; delete[] a; return n; } }
class Program { public static int Main() { return Sink.Take(new Payload[2]) + 40; } }''', 42),
    'callee lets its owned array parameter drop': (False, PAYLOAD + '''class Sink {
 public static int Take(owned Payload[] a) { return a.Length; } }
class Program { public static int Main() { owned Payload[] a = new Payload[2]; a[0] = new Payload(1);
 a[1] = new Payload(2); int n = Sink.Take(move(a)); return Payload.Drops * 10 + n + 20; } }''', 42),
    'owned array parameter used after delete[]': (False, '''class Sink { public static int Take(owned int[] a) {
 delete[] a; return a.Length; } }
class Program { public static int Main() { return Sink.Take(new int[2]); } }''',
        "use of moved-from parameter 'a'"),
    'owned array parameter deleted twice': (False, '''class Sink { public static void Take(owned int[] a) {
 delete[] a; delete[] a; } }
class Program { public static int Main() { Sink.Take(new int[2]); return 42; } }''',
        "use of moved-from parameter 'a'"),
    'caller uses an array after moving it': (False, PAYLOAD + '''class Sink { public static int Take(owned Payload[] a)
 { delete[] a; return 0; } }
class Program { public static int Main() { owned Payload[] a = new Payload[1]; a[0] = new Payload(1);
 Sink.Take(move(a)); return a.Length; } }''', "use of moved-from RAII local 'a'"),
    'method call on a temporary struct receiver': (False, '''struct Elapsed { public long Ticks;
 public double TotalMilliseconds() { return Ticks / 10.0; } }
class Watch { public long T = 420; public Elapsed Read() { Elapsed s = new Elapsed(); s.Ticks = T; return s; } }
class Program { public static int Main() { raii Watch w = new Watch(); return (int)w.Read().TotalMilliseconds(); } }''',
        42),
    'generic parameter as delegate return type': (False, '''delegate T Producer<T>(); class Program { static int
 Make() { return 42; } public static int Main() { Producer<int> p = Make; return p(); } }''', 42),
    'borrowed field': (False, PAYLOAD + '''class Holder { private borrowed Payload slot; }
class Program { public static int Main() { return 0; } }''',
        "borrowed field 'slot' is rejected"),
    'plain reference field that does not own its target': (False, PAYLOAD + '''class Lock { private Payload target;
 public Lock(Payload p) { target = p; } public int Read() { return target.Value; } }
class Program { public static int Main() { raii Payload p = new Payload(42); raii Lock l = new Lock(p);
 return l.Read(); } }''', 42),
    'buffer growth while a view of it is alive': (True, USING + '''class Program { public static int Main() {
 raii ByteBuffer b = new ByteBuffer(1); b.Add(1); Span<byte> s = b.AsSpan(); b.Add(2); return s[0] + 41; } }''',
        'cannot call potentially mutating method while a borrowed field view is alive'),
    'buffer growth after the view ended': (True, USING + '''class Program { public static int Main() {
 raii ByteBuffer b = new ByteBuffer(1); b.Add(41); int r = 0; { Span<byte> s = b.AsSpan(); r = s[0]; } b.Add(2);
 return r + 1; } }''', 42),
    'static members of closed generic types': (False, '''class Box<T> { public static int Count = 40;
 public static int Bump() { Count = Count + 2; return Count; } }
class Program { public static int Main() { Box<int>.Bump(); Box<long>.Bump(); return Box<int>.Count; } }''', 42),
    'Optional factory on the closed generic type': (True, USING + '''class Program { public static int Main() {
 Optional<int> o = Optional<int>.None(); return 0; } }''', "has no compatible member named 'None'"),
    'Optional factory on Optionals': (True, USING + '''class Program { public static int Main() {
 Optional<int> o = Optionals.None<int>(); Optional<int> s = Optionals.Some<int>(42);
 return o.HasValue() ? 1 : s.ValueOr(0); } }''', 42),
    'Optional of a view': (True, USING + '''class Program { public static int Main() {
 Optional<StringView> o = Optionals.None<StringView>(); return 0; } }''',
        "view type 'type:NativeSharpCore::NativeSharp.Core.StringView'"),
    'address of an indexed string character': (True, USING + '''class Program { public static int Main() {
 string s = "abc"; char* p = &s[0]; return 0; } }''', 'an indexed character is a value, not addressable storage'),
    'address of a string character through CharacterPointer': (True, USING + '''class Program {
 public static int Main() { string s = "abc"; char* p = String.CharacterPointer(s, 1); return (int)p[0] - 56; } }''',
        42),
    'address of a by-value indexer result': (False, '''class Grid { public int this[int i] { get { return i; } } }
class Program { public static int Main() { raii Grid g = new Grid(); int* p = &g[0]; return 0; } }''',
        'an indexer that returns by value yields a temporary'),
    'address of a bit field keeps its diagnostic': (False, '''struct Bits { public uint Value : 3; }
class Program { static uint* Address(Bits* value) { return &value->Value; } public static int Main() { return 0; } }''',
        'bit fields have no address'),
}

PROJECT = ('<Project><PropertyGroup><Name>P</Name><OutputType>Exe</OutputType></PropertyGroup>'
           '<ItemGroup><Compile Include="Program.ns" />{}</ItemGroup></Project>')
REFERENCES = ('<LibraryReference Include="{0}/NativeSharpCore.lib" Metadata="{0}/NativeSharpCore.nsmeta" />'
              '<LibraryReference Include="{0}/Generic.lib" Metadata="{0}/Generic.nsmeta" />')


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=180)


def build_libraries(compiler, root):
    library_sources = Path(sys.argv[2]).resolve() / 'stdlib'
    libraries = root / 'libraries'
    libraries.mkdir()
    shutil.copytree(library_sources / 'NativeSharp.Core', libraries / 'core', ignore=shutil.ignore_patterns('obj', 'bin'))
    core = run([compiler, '--no-artifact-cache', '--emit-project-oop-lib',
                str(libraries / 'core/NativeSharp.Core.nsproj'), '-o', str(libraries / 'NativeSharpCore.lib')],
               libraries)
    if core.returncode:
        raise RuntimeError('Core library: ' + (core.stdout + core.stderr)[-600:])
    shutil.copytree(library_sources / 'NativeSharp.Core.Collections.Generic', libraries / 'generic',
                    ignore=shutil.ignore_patterns('obj', 'bin'))
    generic = libraries / 'generic/NativeSharp.Core.Collections.Generic.nsproj'
    built = run([compiler, '--no-artifact-cache', '--library-reference-root=' + str(libraries),
                 '--emit-project-oop-lib', str(generic), '-o',
                 str(libraries / 'Generic.lib')], libraries)
    if built.returncode:
        raise RuntimeError('Collections.Generic library: ' + (built.stdout + built.stderr)[-600:])
    return libraries


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-library-antecedents-') as temporary:
        root = Path(temporary)
        os.environ['TEMP'] = os.environ['TMP'] = temporary
        libraries = build_libraries(compiler, root).as_posix()
        for index, (name, (uses_libraries, source, expected)) in enumerate(CASES.items()):
            folder = root / ('case-' + str(index))
            folder.mkdir()
            (folder / 'Program.ns').write_text(source, encoding='utf-8')
            project = folder / 'Case.nsproj'
            project.write_text(PROJECT.format(REFERENCES.format(libraries) if uses_libraries else ''),
                               encoding='utf-8')
            if isinstance(expected, str):
                result = run([compiler, '--emit-project-oop-exe', str(project), '-o', str(folder / 'rejected.exe'),
                              '--no-artifact-cache', '--no-error-log'], folder)
                output = result.stdout + result.stderr
                print(name, 'exit', result.returncode, flush=True)
                if result.returncode == 0 or expected not in output:
                    failures.append((name, 'rejection', output.strip()[-300:]))
                continue
            for level in ('O0', 'O1', 'O2', 'O3'):
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
    print('library_antecedent_boundaries: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
