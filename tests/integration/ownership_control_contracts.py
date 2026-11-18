"""Ownership, references, cleanup, conversions and strings: the contracts behind LKF-005..LKF-390.

The umbrella records LKF-005, 006, 007, 008, 015, 016 and 026 ask to corroborate behaviour before
attributing a defect, and LKF-154..170 record declared limits of the front-end model. Each program
here either exits with its expected code at O0 and O2 or is rejected with the exact diagnostic, and
each rejection has an accepted control.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

CORE = 'using NativeSharp.Core;\n'
P = 'class P { public static int Drops = 0; public int V = 1; ~P() { Drops = Drops + 1; } }\n'
HOLDER = 'class H { public owned P Item { get; set; } }\n'


def main_body(body):
    return 'class Program { public static int Main() { ' + body + ' } }'


CASES = {
    # LKF-005
    'struct with an owned field is not copied': (False, P + 'struct S { public owned P Item; }\n' +
        main_body('S a; a.Item = new P(); S b = a; return 42;'),
        "cannot be copied into local 'b'; use move(source)"),
    'struct with an owned field is moved': (False, P + 'struct S { public owned P Item; }\n' +
        main_body('{ S a; a.Item = new P(); S b = move(a); } return 41 + P.Drops;'), 42),
    'owned element extracted by move': (False, P + main_body('{ owned P[] a = new P[2]; a[0] = new P(); '
        'owned P x = move(a[0]); delete[] a; } return 41 + P.Drops;'), 42),
    'owned element replaced': (False, P + main_body('{ owned P[] a = new P[1]; a[0] = new P(); a[0] = new P(); '
        'delete[] a; } return 40 + P.Drops;'), 42),
    'owned property replaced': (False, P + HOLDER + main_body('{ raii H h = new H(); h.Item = new P(); '
        'h.Item = new P(); } return 40 + P.Drops;'), 42),
    'owned getter alias inside the owner scope': (False, P + HOLDER + main_body('raii H h = new H(); '
        'h.Item = new P(); P a = h.Item; return 41 + a.V;'), 42),
    # LKF-006
    'ref return of a local': (False, 'class Program { static ref int R() { int x = 1; return ref x; } '
        'public static int Main() { return 42; } }',
        'ref return cannot expose storage owned by the current stack frame'),
    'delete of an array with a live element reference': (False, main_body('int[] a = new int[2]; ref int r = ref a[0]; '
        'delete[] a; r = 1; return 42;'),
        "cannot delete[] array 'a' while reference 'r' to one of its elements is alive"),
    'lambda capture of a raii owner': (False, P + main_body('raii P p = new P(); var f = () => p.V; return 42;'),
        "RAII owner 'p' cannot be captured by a lambda without an explicit move capture"),
    # LKF-007
    'cleanup on break and continue': (False, P + main_body('for (int i = 0; i < 4; i = i + 1) { raii P p = new P(); '
        'if (i == 1) { continue; } if (i == 2) { break; } } return 39 + P.Drops;'), 42),
    'cleanup on return': (False, P + 'class Program { static int F() { raii P p = new P(); return 41; } '
        'public static int Main() { int r = F(); return r + P.Drops; } }', 42),
    'cleanup and finally on a throw': (True, CORE + P + 'class Program { static int Log = 0; static void F() '
        'throws Exception { raii P p = new P(); try { throw new Exception(); } finally { Log = Log + 1; } } '
        'public static int Main() { try { F(); } catch (Exception e) { } return 40 + Log + P.Drops; } }', 42),
    'foreach break runs the generator finally': (True, CORE + 'class Program { static int Done = 0; '
        'static Iterator<int, int> Gen() { try { yield return 1; yield return 2; yield return 3; } finally '
        '{ Done = Done + 1; } } public static int Main() { int t = 0; foreach (int x in Gen()) { t = t + x; '
        'if (x == 2) { break; } } return 39 + t + Done - 1; } }', 42),
    'owner transfer requires an owned parameter': (False, P + 'class Q : P { }\nclass Program { '
        'static void Kill(P p) '
        '{ delete p; } public static int Main() { owned P q = new Q(); Kill(move(q)); return 42; } }',
        'ownership-transfer argument requires an owned parameter'),
    # LKF-008
    'integer overflow wraps': (False, main_body('int m = 2147483647; int r = m + 1; '
        'return r == -2147483648 ? 42 : 1;'), 42),
    'checked overflow traps': (False, main_body('int m = 2147483647; int r = checked(m + 1); return 42;'), 147),
    'division by zero traps': (False, main_body('int z = 0; int r = 10 / z; return 42;'), 129),
    'double out of int range traps': (False, main_body('double d = 1e20; int r = (int)d; return 42;'), 133),
    'operator operands evaluated left to right': (False, 'struct V { public int A; '
        'public static V operator +(V x, V y) { V r; r.A = x.A + y.A; return r; } }\nclass Program { '
        'static int Order = 0; static V Mk(int a) { Order = Order * 10 + a; V v; v.A = a; return v; } '
        'public static int Main() { V s = Mk(1) + Mk(2); '
        'return Order == 12 && s.A == 3 ? 42 : 1; } }', 42),
    # LKF-015
    'flexible tail struct passed by value copies the prefix': (False, 'struct Packet { public int Count; '
        'public int[*] Values; }\nclass Program { static int Use(Packet p) { return p.Count; } '
        'public static int Main() { Packet p; p.Count = 42; return Use(p); } }', 42),
    'struct with only a flexible tail': (False, 'struct OnlyTail { public int[*] Values; }\n' + main_body('return 42;'),
        42),
    # LKF-016
    'negative string index traps': (True, CORE + main_body('string s = "abc"; int i = -1; char c = s[i]; return 42;'),
        132),
    'length of a null string traps': (True, CORE + main_body('string s = null; return s.Length;'), 135),
    'owned string local': (True, CORE + main_body('owned string s = "ab" + "c"; return s.Length + 39;'), 42),
    'embedded NUL counts in the length': (True, CORE + main_body('string s = "a\\0b"; return s.Length + 39;'), 42),
    'UTF-8 text length counts bytes': (True, CORE + main_body('string s = "é"; return s.Length + 40;'), 42),
    # LKF-026
    'process exit skips scope cleanup': (True, CORE + P + main_body('raii P p = new P(); Environment.Exit(42); '
        'return 1;'), 42),
    # LKF-154
    'error limit reports suppressed diagnostics': (False, 'class Program { public static int Main() { ' +
        ' '.join('int x%d = undefined%d;' % (index, index) for index in range(150)) + ' return 0; } }',
        'suppressed'),
    # LKF-166
    'new passed to an ordinary parameter': (False, P + 'class Program { static int Use(P p) { return p.V; } '
        'public static int Main() { return Use(new P()); } }', 'new'),
    'new passed to an owned parameter': (False, P + 'class Program { static int Use(owned P p) { return p.V + 41; } '
        'public static int Main() { return Use(new P()); } }', 42),
    # LKF-169
    'aggregate fields start at zero': (False, 'struct S { public int A; public int B; }\n' +
        main_body('S s; s.A = 40; return s.A + s.B + 2;'), 42),
    'scalar local must be assigned': (False, main_body('int x; return x;'), "use of unassigned local variable 'x'"),
    # LKF-170
    'compound property assignment through a call': (False, 'class H { public int V { get; set; } }\n'
        'class Program { static H Pick(H h) { return h; } public static int Main() { raii H h = new H(); '
        'Pick(h).V += 41; return h.V; } }', 'assign this one to a local first and use that local'),
    'compound property assignment through a local': (False, 'class H { public int V { get; set; } }\n'
        'class Program { public static int Main() { raii H h = new H(); h.V += 42; return h.V; } }', 42),
    # LKF-333
    'reference from a call returning a parameter reference': (False, 'class Program { static ref int Pick(ref int a) '
        '{ return ref a; } public static int Main() { int x = 1; ref int r = ref Pick(ref x); r = 42; return x; } }',
        42),
    'reference from a call over a local does not escape': (False, 'class Program { static ref int Pick(ref int a) '
        '{ return ref a; } static ref int Bad() { int y = 1; return ref Pick(ref y); } '
        'public static int Main() { return 42; } }',
        'ref return cannot expose storage owned by the current stack frame'),
    'local reference over a local does not escape': (False, 'class Program { static ref int Pick(ref int a) '
        '{ return ref a; } static ref int Bad() { int y = 1; ref int r = ref Pick(ref y); return ref r; } '
        'public static int Main() { return 42; } }',
        'ref return cannot expose storage owned by the current stack frame'),
    # LKF-334
    'borrowed local from a live owner': (False, P + main_body('raii P p = new P(); borrowed P b = p; return 41 + b.V;'),
        42),
    'borrowed local from a call result': (False, P + 'class Program { static P Make(P p) { return p; } '
        'public static int Main() { raii P p = new P(); borrowed P b = Make(p); return b.V; } }',
        'borrowed local requires a direct live owner or borrowed identifier source'),
    # LKF-390
    'borrow of a raii owner across an await': (False, P + 'class Program { static async int Two() { return 2; } '
        'public static async int Main() { raii P p = new P(); borrowed P b = p; int t = await Two(); '
        'return 39 + b.V + t; } }', 42),
    # NSC-NCSS-004
    'raii class local replaced by an owned operator result': (False, 'class C { public int A; '
        'public C(int a) { A = a; } public static owned C operator +(C x, C y) { return new C(x.A + y.A); } }\n'
        'class Program { public static int Main() { owned C a = new C(40); raii C b = new C(2); a = a + b; '
        'return a.A; } }', 42),
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
    with tempfile.TemporaryDirectory(prefix='nsc-ownership-control-') as temporary:
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
