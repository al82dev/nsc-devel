"""Owner reassignment, repeated field moves, borrow composition and raw pointer contracts.

Fichas LKF-018, LKF-102, LKF-103 and LKF-104. Each program either exits 42 at O0 and O2 or is
rejected with the exact diagnostic, and every rejection has an accepted control. One restriction was
lifted with these cases: a RAII local (also a local bound to `new`, item 141) accepted only
`target = move(source)`, so `a = new T(...)` was rejected. A fresh owner -- `new T(...)`, `new T[n]`,
`null` or an owned-returning call -- now replaces the owner like a move does: the previous object is
destroyed before the store and a moved-from local becomes live again (LKF-018/103). A plain call
result is still rejected, as is overwriting an owner while a borrow of it is alive. Binding a borrowed
local from a borrowed return stays a boundary with an explicit diagnostic; the result is used
inline instead (LKF-104). Unqualified raw pointers are the raw tier: null, alignment and bounds are
not checked; the `*!` and `align(n)` qualifiers are checked statically (LKF-102).
"""
from pathlib import Path
import subprocess
import sys
import tempfile

RESOURCE = '''class R { public static int Drops = 0; public int V; public R(int v) { V = v; }
 ~R() { Drops = Drops + 1; } }
class F { public static owned R Make(int v) { return new R(v); } public static R Plain(R r) { return r; } }
'''

CASES = {
    'second move of an owned field finds null': (RESOURCE + '''class H { public owned R f; }
class Program { public static int Main() { int r = 0; { raii H h = new H(); h.f = new R(5); raii R x = move(h.f);
 R y = h.f; r = (y == null ? 30 : 1) + x.V; } return r + R.Drops * 7; } }''', 42),
    'moved-from RAII local reactivated with new': (RESOURCE + '''class Program { public static int Main() {
 int r = 0; { raii R a = new R(1); raii R b = move(a); a = new R(2); r = a.V + b.V; } return r * 10 + R.Drops + 10;
 } }''', 42),
    'moved-from RAII local reactivated with an owned call': (RESOURCE + '''class Program { public static int Main() {
 int r = 0; { raii R a = F.Make(1); raii R b = move(a); a = F.Make(2); r = a.V + b.V; } return r * 10 + R.Drops + 10;
 } }''', 42),
    'RAII local replaced with new destroys the previous owner': (RESOURCE + '''class Program {
 public static int Main() { int d = 0; { raii R a = new R(1); a = new R(2); d = R.Drops; } return d * 10 + R.Drops + 30;
 } }''', 42),
    'local bound to new replaced with new': (RESOURCE + '''class Program { public static int Main() { int d = 0;
 int v = 0; { R a = new R(1); a = new R(2); d = R.Drops; v = a.V; } return d * 10 + R.Drops + v + 28; } }''', 42),
    'RAII local replaced with an owned call': (RESOURCE + '''class Program { public static int Main() { int d = 0;
 { raii R a = F.Make(1); a = F.Make(2); d = R.Drops; } return d * 10 + R.Drops + 30; } }''', 42),
    'RAII local cleared with null': (RESOURCE + '''class Program { public static int Main() { int d = 0;
 { raii R a = new R(1); a = null; d = R.Drops; } return d * 10 + R.Drops + 31; } }''', 42),
    'RAII local replaced in a loop': (RESOURCE + '''class Program { public static int Main() { int i = 0;
 { raii R a = new R(0); while (i < 3) { a = new R(i); i = i + 1; } } return R.Drops * 10 + 2; } }''', 42),
    'RAII array replaced with new destroys its elements': (RESOURCE + '''class Program { public static int Main() {
 int d = 0; int n = 0; { raii R[] a = new R[1]; a[0] = new R(1); a = new R[2]; d = R.Drops; a[1] = new R(2);
 n = a.Length; } return d * 10 + R.Drops + n + 28; } }''', 42),
    'RAII scalar array replaced with new': ('''class Program { public static int Main() { int n = 0;
 { raii int[] a = new int[2]; a = new int[3]; n = a.Length; } return n + 39; } }''', 42),
    'RAII local replaced with a plain call result': (RESOURCE + '''class Program { public static int Main() {
 raii R a = new R(1); raii R b = new R(2); a = F.Plain(b); return 0; } }''',
        'RAII locals are move-only; assignment requires target = move(source), new T(...)'),
    'RAII local replaced while a borrow of it is alive': (RESOURCE + '''class Program { public static int Main() {
 raii R a = new R(1); borrowed R b = a; a = new R(2); return b.V; } }''',
        "cannot overwrite owner 'a' while a borrowed local is alive"),
    'RAII local replaced after the borrow ended': (RESOURCE + '''class Program { public static int Main() {
 raii R a = new R(1); int v = 0; { borrowed R b = a; v = b.V; } a = new R(41); return a.V + v; } }''', 42),
    'self move-assignment': (RESOURCE + '''class Program { public static int Main() { raii R a = new R(1); a = move(a);
 return 0; } }''', 'self move-assignment of a RAII local is not allowed'),
    'borrowed return used inline': (RESOURCE + '''class G { public static borrowed R Pick(borrowed R a, borrowed R b,
 bool first) { return first ? a : b; } }
class Program { public static int Main() { raii R a = new R(40); raii R b = new R(2);
 return G.Pick(a, b, true).V + b.V; } }''', 42),
    'reborrow through nested calls': (RESOURCE + '''class G { public static int Read(borrowed R r) { return r.V; }
 public static int Twice(borrowed R r) { return Read(r) + Read(r); } }
class Program { public static int Main() { raii R a = new R(21); return G.Twice(a); } }''', 42),
    'borrowed local from a live owner': (RESOURCE + '''class Program { public static int Main() {
 raii R a = new R(42); borrowed R b = a; return b.V; } }''', 42),
    'borrowed local bound to a borrowed return': (RESOURCE + '''class G { public static borrowed R Pick(borrowed R a)
 { return a; } }
class Program { public static int Main() { raii R a = new R(42); borrowed R c = G.Pick(a); return c.V; } }''',
        'borrowed local requires a direct live owner or borrowed identifier source'),
    'borrowed local bound to a return with a named lifetime': (RESOURCE + '''class G {
 public static borrowed[a] R Pick(borrowed[a] R x) { return x; } }
class Program { public static int Main() { raii R a = new R(42); borrowed R c = G.Pick(a); return c.V; } }''',
        'borrowed local requires a direct live owner or borrowed identifier source'),
    'borrowed return of an owned field': (RESOURCE + '''class H { public owned R f;
 public borrowed R Get() { return f; } }
class Program { public static int Main() { return 42; } }''',
        'borrowed return requires a borrowed value whose lifetime outlives the call'),
    'nonnull pointer from an address': ('''class Program { static int Read(int*! p) { return *p; }
 public static int Main() { int value = 42; int*! p = &value; return Read(p); } }''', 42),
    'nonnull pointer from null': ('''class Program { public static int Main() { int*! p = null; return 0; } }''',
        'pointer initializer does not satisfy'),
    'alignment that the source does not prove': ('''class Program { public static int Main() { int value = 0;
 int*! source = &value; int*! align(16) target = source; return 0; } }''', 'pointer initializer does not satisfy'),
    'unaligned access through a raw pointer': ('''class Program { public static int Main() {
 byte* b = stackalloc byte[16]; int* p = (int*)(b + 1); *p = 42; return *p; } }''', 42),
}

PROJECT = ('<Project><PropertyGroup><Name>P</Name><OutputType>Exe</OutputType>'
           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" /></ItemGroup></Project>')


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=120)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-owner-reassignment-') as temporary:
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
    print('owner_reassignment_borrow_boundaries: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
