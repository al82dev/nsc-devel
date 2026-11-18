"""Parser and front-end restrictions at the edge of the NativeSharp surface (LKF-509..LKF-584).

Each program either exits 42 at O0 and O2 or is rejected with the exact diagnostic, and every
rejection has an accepted control. The cases pin the documented restrictions (static namespace
declarations, explicit offsets, variant payloads and layout, generic trait methods, promoted
parameters, runtime interpolation formats, literal bytes and identifiers) and the depth and arity
the front end handles (300 arguments, 300 nested generic arguments and aggregates, list pattern
rest captures).
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

CORE = 'using NativeSharp.Core;\n'


def main_body(body):
    return 'class Program { public static int Main() { ' + body + ' } }'


def parameters(count):
    return ', '.join('int a%d' % index for index in range(count))


def ones(count):
    return ', '.join('1' for _ in range(count))


def generic_nesting(depth):
    spelled = 'int'
    for _ in range(depth):
        spelled = 'Box<' + spelled + '>'
    return 'struct Box<T> { public T V; }\n' + main_body(spelled + ' b; return 42;')


def aggregate_nesting(depth):
    types = 'struct S0 { public int V; }\n' + ''.join(
        'struct S%d { public S%d A; }\n' % (index, index - 1) for index in range(1, depth + 1))
    initializer = '{' * (depth + 1) + '42' + '}' * (depth + 1)
    return types + main_body('S%d x = %s; return x%s.V;' % (depth, initializer, '.A' * depth))


FORMAT = ('runtime interpolation format must be D or X with optional width, F with optional precision, '
          'or G without precision; numeric arguments are limited to 64')
LITERAL_BYTES = 'interpolated string literal contains an invalid control byte or UTF-8 sequence'

CASES = {
    # LKF-509
    'static call with 300 arguments': (False, 'class Program { static int F(' + parameters(300) +
        ') { return a0 + a299 + 40; } public static int Main() { return F(' + ones(300) + '); } }', 42),
    'virtual call with 300 arguments': (False, 'class B { public virtual int F(' + parameters(300) +
        ') { return a0 + a299 + 40; } }\nclass Program { static int Run(B b) { return b.F(' + ones(300) +
        '); } public static int Main() { raii B b = new B(); return Run(b); } }', 42),
    'constructor with 300 arguments': (False, 'class K { public int V; public K(' + parameters(300) +
        ') { V = a0 + a299 + 40; } }\n' + main_body('raii K k = new K(' + ones(300) + '); return k.V;'), 42),
    # LKF-516
    'generic argument nested 300 times': (False, generic_nesting(300), 42),
    'aggregate initializer nested 300 times': (False, aggregate_nesting(300), 42),
    # LKF-533
    'list pattern rest capture': (True, CORE + main_body('int[] a = new int[5]; a[0] = 1; a[2] = 40; a[4] = 1; '
        'int r = match (a) { [var x, .. var rest, var y] => x + rest.Length() + y + rest[1] - 3, _ => 0 }; '
        'delete[] a; return r;'), 42),
    'list pattern rest capture on a short array': (True, CORE + main_body('int[] a = new int[1]; '
        'int r = match (a) { [var x, .. var rest, var y] => rest.Length(), _ => 42 }; delete[] a; return r;'), 42),
    'string pattern rest capture': (True, CORE + main_body('string s = "abcde"; '
        'int r = match (s) { [var x, .. var rest, var y] => rest.Length() + 39, _ => 0 }; return r;'), 42),
    # LKF-538
    'static namespace declaration': (False, 'namespace N { static int X = 1; }\n' + main_body('return 42;'),
        'static namespace declarations/file-local linkage are not supported in NativeSharp'),
    'namespace state': (False, 'namespace N { public int X = 42; }\n' + main_body('return N.X;'), 42),
    # LKF-539
    'explicit offset past 16 MiB': (False, 'struct S { at(16777216) public int X; }\n' + main_body('return 42;'),
        'explicit field offset is out of range'),
    'explicit offset inside the limit': (False, 'struct S { at(0) public int A; at(4096) public byte X; }\n' +
        main_body('S s; s.X = 42; return s.X;'), 42),
    # LKF-543 / LKF-560
    'borrowed variant payload': (False, 'variant V { A(borrowed int*), B }\n' + main_body('return 42;'),
        'borrowed variant payloads are outside the NativeSharp storage contract'),
    'variant payload': (False, 'variant V { A(int*), B }\n' + main_body('return 42;'), 42),
    'packed variant': (False, 'pack(1) variant V { A(int), B }\n' + main_body('return 42;'),
        'pack/align/size cannot override the canonical tagged-variant layout'),
    # LKF-544
    'generic trait method': (False, 'trait T { int F<U>(U u); }\n' + main_body('return 42;'),
        'generic trait methods are not object-safe in the current T05 vertical'),
    'generic trait': (False, 'trait T<U> { int F(U u); }\nstruct S : T<int> { public int F(int u) { return u; } }\n' +
        main_body('S s; return s.F(42);'), 42),
    # LKF-546
    'promoted ref parameter': (False, 'struct P(ref int X) { }\n' + main_body('return 42;'),
        'a promoted parameter cannot be params, ref, out, or in'),
    'promoted value parameter': (False, 'struct P(public int X) { }\n' + main_body('P p = new P(42); return p.X;'),
        42),
    # LKF-567
    'runtime interpolation D4': (True, CORE + main_body('int x = 4; raii string s = $"{x:D4}"; '
        'return s.Length + 38;'), 42),
    'runtime interpolation F2': (True, CORE + main_body('double x = 4.5; raii string s = $"{x:F2}"; '
        'return s.Length + 38;'), 42),
    'runtime interpolation X': (True, CORE + main_body('int x = 255; raii string s = $"{x:X}"; '
        'return s.Length + 40;'), 42),
    'runtime interpolation N2': (True, CORE + main_body('int x = 4; raii string s = $"{x:N2}"; return 42;'), FORMAT),
    'runtime interpolation G2': (True, CORE + main_body('double x = 4.5; raii string s = $"{x:G2}"; return 42;'),
        FORMAT),
    'runtime interpolation width 65': (True, CORE + main_body('int x = 4; raii string s = $"{x:D65}"; '
        'return 42;'), FORMAT),
    # LKF-582
    'interpolated literal with a control byte': (True, CORE + main_body('int x = 1; raii string s = $"a\x01{x}"; '
        'return 42;'), LITERAL_BYTES),
    'interpolated literal with an invalid UTF-8 byte': (True, CORE.encode() + b'class Program { public static '
        b'int Main() { int x = 1; raii string s = $"a\xff{x}"; return 42; } }', LITERAL_BYTES),
    'interpolated literal with UTF-8 text': (True, CORE + main_body('int x = 1; raii string s = $"é{x}"; '
        'return s.Length + 39;'), 42),
    'string literal with a control byte': (False, main_body('string s = "a\x01"; return 42;'),
        'invalid or unterminated string literal'),
    # LKF-583
    'identifier with a non-ASCII letter': (False, main_body('int café = 42; return café;'),
        'unexpected character in source'),
    'ASCII identifier': (False, main_body('int cafe = 42; return cafe;'), 42),
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
    with tempfile.TemporaryDirectory(prefix='nsc-frontend-restrictions-') as temporary:
        root = Path(temporary)
        core = build_core(compiler, root)
        for index, (name, (needs_library, source, expected)) in enumerate(CASES.items()):
            folder = root / ('case-' + str(index))
            folder.mkdir()
            data = source if isinstance(source, bytes) else source.encode('utf-8')
            (folder / 'Program.ns').write_bytes(data)
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
