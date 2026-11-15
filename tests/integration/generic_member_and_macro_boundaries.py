"""Generics, members, construction and macros at the edge of the supported surface (LKF-346..LKF-388).

Each program either exits 42 at O0 and O2 or is rejected with the exact diagnostic, and every
rejection has an accepted control. Defects fixed with these cases:

- A constructor written without an initializer called base() without the defaults of the selected
  base constructor, and the LLVM verifier rejected the call (LKF-346).
- A generic extension method called with arguments matched the first argument with the receiver
  parameter, so inference failed (LKF-355).
- Fixed-array type arguments were spelled `[fixed]` without their extent, so Box<int[2]> and
  Box<int[8]> shared one specialization and its layout (LKF-371).
- A string += on a destination other than a raii string local gave a numeric diagnostic (LKF-376).
- The macro depth limit counted every invocation, so 33 independent invocations failed (LKF-382).
- `#macro` and `#expand` inside a string literal or a comment were taken as directives (LKF-383).
- An integer default value was not range-checked: `byte x = 300` passed 44 and `uint x = -1` passed
  4294967295 (LKF-384).
- `owned` in a tuple element type was dropped silently, and a new object or a transferred owner stored
  in a tuple element leaked (LKF-388).

The other cases pin verified boundaries with their controls.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

MAIN = 'class Program { public static int Main() { BODY } }'
CORE = 'using NativeSharp.Core;\n'
BASE = 'class B { public int V; public B(int v = 42) { V = v; } }\n'
BOX = 'struct Box<T> { public T V; }\n'
ONE = '#macro one(x) { $x }\n'
DROPS = 'class P { public static int Drops = 0; public int V = 41; ~P() { Drops = Drops + 1; } }\n'
VALUES = 'trait IValue { type A; type B; }\nclass Number { }\n'

CASES = {
    # LKF-346
    'constructor without an initializer over a defaulted base': (False, BASE + 'class D : B { public D() { } }\n' +
        MAIN.replace('BODY', 'D d = new D(); return d.V;'), 42),
    'explicit base() over a defaulted base': (False, BASE + 'class D : B { public D() : base() { } }\n' +
        MAIN.replace('BODY', 'D d = new D(); return d.V;'), 42),
    'implicit constructor over a defaulted base': (False, BASE + 'class D : B { }\n' +
        MAIN.replace('BODY', 'D d = new D(); return d.V;'),
        'an implicit constructor passes no default arguments, so declare a constructor that calls base()'),
    'implicit constructor over a parameterless base': (False, 'class B { public int V; public B() { V = 1; } '
        'public B(int v = 42) { V = v; } }\nclass D : B { }\n' +
        MAIN.replace('BODY', 'D d = new D(); return d.V + 41;'), 42),
    'this() over a defaulted constructor': (False, 'class B { public int V; public B(int v = 42) { V = v; } '
        'public B(long w) : this() { } }\n' + MAIN.replace('BODY', 'B b = new B(1L); return b.V;'), 42),
    # LKF-349
    'associated type cycle': (False, VALUES + 'impl IValue for Number { type A = Number.B; type B = Number.A; }\n' +
        MAIN.replace('BODY', 'return 42;'), 'associated type normalization cycle'),
    'associated projection inside a type argument': (False, VALUES + 'class Holder<T> { public T V; }\n'
        'impl IValue for Number { type A = Holder<Number.B>; type B = int; }\n' + MAIN.replace('BODY', 'return 42;'),
        "unknown type name 'Number.B'"),
    # LKF-355
    'generic extension method with an argument': (False, 'struct Box { public int V; }\nextension Box { '
        'public T Pick<T>(T v) { return v; } }\n' + MAIN.replace('BODY', 'Box b; b.V = 40; return b.Pick(2) + b.V;'),
        42),
    'generic extension method with two arguments': (False, 'struct Box { public int V; }\nextension Box { '
        'public int Sum<T>(T v, int w) { return this.V + w; } }\n' +
        MAIN.replace('BODY', 'Box b; b.V = 40; return b.Sum(1L, 2);'), 42),
    'generic extension method with explicit type arguments': (False, 'struct Box { public int V; }\n'
        'extension Box { public int Sum<T>(T v, int w) { return this.V + w; } }\n' +
        MAIN.replace('BODY', 'Box b; b.V = 40; return b.Sum<long>(1L, 2);'), 42),
    # LKF-356 / LKF-362 / LKF-365
    'deeply nested generic inference': (False, 'class G { public static T Id<T>(T v) { return v; } }\n' +
        MAIN.replace('BODY', 'return G.Id(G.Id(G.Id(G.Id(G.Id(G.Id(G.Id(G.Id(42))))))));'), 42),
    'generic inference through a conditional': (False, 'class G { public static T Pick<T>(bool c, T a, T b) '
        '{ return c ? a : b; } }\n' + MAIN.replace('BODY', 'return G.Pick(true, 42, 1);'), 42),
    'generic inference with two argument types': (False, 'class G { public static T Pick<T>(T a, T b) '
        '{ return a; } }\n' + MAIN.replace('BODY', 'long x = 1; return (int)G.Pick(42, x);'),
        'cannot be inferred from the local call arguments'),
    'generic method inside a generic type': (False, 'class Box<T> { public T V; public U Map<U>(U u) '
        '{ return u; } }\n' + MAIN.replace('BODY', 'Box<int> b = new Box<int>(); return b.Map(42);'), 42),
    # LKF-370
    'generic uses at the same offset in two files': (False, {
        'A.ns': 'namespace Q { struct Box<T> { public T V; } }\n',
        'B.ns': 'namespace Q { class F1 { public static int Get() { Box<int> b; b.V = 40; return b.V; } } }\n',
        'C.ns': 'namespace Q { class F2 { public static int Get() { Box<byte> b; b.V = 2; return b.V; } } }\n',
        'Program.ns': 'namespace Q { class Program { public static int Main() { return F1.Get() + F2.Get(); } } }\n'},
        42),
    # LKF-371
    'fixed-array type arguments of different extents': (False, BOX + MAIN.replace('BODY',
        'Box<int[2]> a; Box<int[8]> b; b.V[7] = 40; a.V[1] = 2; return b.V[7] + a.V[1];'), 42),
    'fixed-array type arguments of different sizes': (False, BOX + MAIN.replace('BODY',
        'return (int)(sizeof(Box<int[8]>) - sizeof(Box<int[2]>)) + 18;'), 42),
    'fixed-array type argument indexed out of its extent': (False, BOX + MAIN.replace('BODY',
        'Box<int[8]> b; Box<int[2]> a; a.V[7] = 1; return 42;'), 'constant fixed-array index is outside [0, 2)'),
    # LKF-376
    'string += on a plain string local': (True, CORE + MAIN.replace('BODY', 'string s = "a"; s += "b"; return 42;'),
        'string += needs a raii string local as its target'),
    'string += on an owned string field': (True, CORE + 'class Box { public owned string S = "a"; }\n' +
        MAIN.replace('BODY', 'raii Box b = new Box(); b.S += "b"; return b.S.Length + 40;'),
        "assignment to owned field 'S' requires an ownership-transfer expression"),
    # LKF-377
    'null-conditional index of a two-dimensional array': (False, MAIN.replace('BODY',
        'int[,] p = null; int? v = p?[1, 2]; return v ?? 42;'), 'expected expression'),
    # LKF-382
    'many independent macro invocations': (False, ONE + MAIN.replace('BODY', 'int v = 2; ' +
        ' '.join('v = v + #expand one(1);' for _ in range(40)) + ' return v;'), 42),
    'nested macro invocations': (False, '#macro a(x) { $x }\n#macro b(x) { #expand a($x) }\n'
        '#macro c(x) { #expand b($x) }\n' + MAIN.replace('BODY', 'return #expand c(42);'), 42),
    'macro that expands itself': (False, '#macro again(x) { #expand again($x) }\n' +
        MAIN.replace('BODY', 'return #expand again(1);'), "macro cycle detected: 'again'"),
    # LKF-383
    'macro directive inside a string literal': (False, MAIN.replace('BODY',
        'string s = "#expand nope(1)"; return 42;'), 42),
    'macro directives inside comments': (False, '// #macro hidden(x) { $x }\n/* #expand nope(2) */\n'
        'class Program { public static int Main() { return 42; // #expand nope(1)\n } }', 42),
    'macro invocation after a string with a directive': (False, ONE + MAIN.replace('BODY',
        'string s = "#expand x"; return #expand one(42);'), 42),
    'unknown macro in code': (False, MAIN.replace('BODY', 'return #expand nope(1);'), "unknown macro 'nope'"),
    # LKF-384
    'negative int default': (False, 'class G { public static int F(int x = -42) { return -x; } }\n' +
        MAIN.replace('BODY', 'return G.F();'), 42),
    'byte default out of range': (False, 'class G { public static int F(byte x = 300) { return x; } }\n' +
        MAIN.replace('BODY', 'return G.F();'), "default value 300 does not fit parameter type 'builtin:byte'"),
    'negative unsigned default': (False, 'class G { public static uint F(uint x = -1) { return x; } }\n' +
        MAIN.replace('BODY', 'return (int)G.F();'), "default value -1 does not fit parameter type 'builtin:uint'"),
    'byte default at its maximum': (False, 'class G { public static int F(byte x = 255) { return x - 213; } }\n' +
        MAIN.replace('BODY', 'return G.F();'), 42),
    'sbyte default at its minimum': (False, 'class G { public static int F(sbyte x = -128) { return x + 170; } }\n' +
        MAIN.replace('BODY', 'return G.F();'), 42),
    'named constant default': (False, 'class G { const int K = 42; public static int F(int x = K) { return x; } }\n' +
        MAIN.replace('BODY', 'return G.F();'), 'default parameter value must be a literal constant'),
    # LKF-385
    'impl for a qualified type': (False, 'trait IValue { int Get(); }\nnamespace A { public class Foo { } }\n'
        'namespace B { public class Foo { public int V = 42; } }\nimpl IValue for B.Foo { public int Get() '
        '{ return this.V; } }\n' + MAIN.replace('BODY', 'B.Foo f = new B.Foo(); return f.Get();'),
        'an impl target must be a direct unqualified class or struct type owned by this module'),
    # LKF-386
    'params over a span without the pack layout': (False, 'namespace Mine { public struct ReadOnlySpan<T> '
        '{ public int Length; } }\nclass G { public static int Count(params Mine.ReadOnlySpan<int> xs) '
        '{ return 42; } }\n' + MAIN.replace('BODY', 'return G.Count(1, 2, 3);'),
        "a params pack of 'builtin:int' requires a ReadOnlySpan<T> whose first fields are T* data and int length"),
    # LKF-387
    'static owned automatic property': (False, 'class P { public int V; }\nclass G { public static owned P Item '
        '{ get; set; } }\n' + MAIN.replace('BODY', 'return 42;'), "static property 'Item' cannot be owned"),
    # LKF-388
    'owned element in a tuple type': (False, DROPS + 'struct S { public (owned P, int) T; }\n' +
        MAIN.replace('BODY', 'return 42;'), 'a tuple element cannot be owned'),
    'new object stored in a tuple element': (False, DROPS + MAIN.replace('BODY',
        '{ (P, int) t = (new P(), 1); } return 41 + P.Drops;'), 'an owner cannot be stored in a tuple element'),
    'borrowed object stored in a tuple element': (False, DROPS + MAIN.replace('BODY',
        'raii P p = new P(); (P, int) t = (p, 1); return p.V + 1;'), 42),
}


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=180)


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
    with tempfile.TemporaryDirectory(prefix='nsc-generic-member-') as temporary:
        root = Path(temporary)
        core = build_core(compiler, root)
        for index, (name, (needs_library, source, expected)) in enumerate(CASES.items()):
            folder = root / ('case-' + str(index))
            folder.mkdir()
            files = source if isinstance(source, dict) else {'Program.ns': source}
            for file_name, text in files.items():
                (folder / file_name).write_text(text, encoding='utf-8')
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
