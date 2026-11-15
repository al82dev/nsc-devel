"""Array, variable-length array and stackalloc limits (LKF-004, LKF-012, LKF-022) and null delegates.

Each program either exits with an expected code at O0 and O2 or is rejected with the exact
diagnostic. Trap codes are the runtime protections: 132 index out of range, 134 allocation size
(negative, overflow, over the stack reserve), 135 null reference. Every trap has an in-range control.

- Dynamic arrays guard the count, every index of every dimension, null and the product of the
  dimensions.
- A fixed array rejects a constant index outside its extent. A runtime index is the raw tier by
  design (no bounds check; IndexAndRangeExpressions.cpp), so it is not exercised out of range here.
- Variable-length arrays and stackalloc guard negative, oversized and overflowing extents. They start
  zeroed like fixed arrays, after a call that leaves non-zero data in the same stack area.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

DIRTY = ('static int Dirty() { int[64] junk; int i = 0; while (i < 64) { junk[i] = 77; i = i + 1; } '
         'return junk[3]; }')

CASES = {
    'dynamic negative count': ('static int N() { return -1; } public static int Main() { '
                               'int[] a = new int[N()]; return a.Length; }', 134),
    'dynamic count from long over int range': ('static long N() { return 4294967296l; } public static int Main() { '
                                               'int[] a = new int[N()]; return 0; }', 134),
    'dynamic negative index': ('static int I() { return -1; } public static int Main() { '
                               'int[] a = new int[2]; return a[I()]; }', 132),
    'dynamic upper index': ('static int I() { return 2; } public static int Main() { '
                            'int[] a = new int[2]; return a[I()]; }', 132),
    'dynamic in-range index': ('static int I() { return 1; } public static int Main() { '
                               'int[] a = new int[2]; a[I()] = 42; return a[1]; }', 42),
    'dynamic null length': ('static int[] Get() { return null; } public static int Main() { '
                            'int[] a = Get(); return a.Length; }', 135),
    'dynamic dimension product overflow': ('static int N() { return 2147483647; } public static int Main() { '
                                           'int[,] a = new int[N(), N()]; return 0; }', 134),
    'dynamic second dimension index': ('static int J() { return 3; } public static int Main() { '
                                       'int[,] a = new int[2, 3]; return a[1, J()]; }', 132),
    'dynamic two dimensions in range': ('public static int Main() { int[,] a = new int[2, 3]; '
                                        'a[1, 2] = 42; return a[1, 2]; }', 42),
    'fixed constant upper index': ('public static int Main() { int[4] values; return values[4]; }',
                                   'constant fixed-array index is outside [0, 4)'),
    'fixed constant negative index': ('public static int Main() { int[4] values; return values[-1]; }',
                                      'constant fixed-array index is outside [0, 4)'),
    'fixed runtime index in range': ('static int I() { return 3; } public static int Main() { '
                                     'int[4] values; values[I()] = 42; return values[3]; }', 42),
    'fixed array starts zeroed': (DIRTY + ' static int Read() { int[64] values; int sum = 0; int i = 0; '
                                  'while (i < 64) { sum = sum + values[i]; i = i + 1; } return sum; } '
                                  'public static int Main() { Dirty(); return Read() == 0 ? 42 : 1; }', 42),
    'variable-length negative extent': ('static int Use(int count) { int[count] values; return 0; } '
                                        'public static int Main() { return Use(-1); }', 134),
    'variable-length extent over the stack reserve': ('static int Use(int count) { int[count] values; return 0; } '
                                                      'public static int Main() { return Use(262145); }', 134),
    'variable-length extent product overflow': ('static int Use(long rows, long columns) { '
                                                'byte[rows, columns] values; return 0; } public static int Main() { '
                                                'return Use(1099511627776L, 1099511627776L); }', 134),
    'variable-length extent in range': ('static int Use(int count) { int[count] values; values[count - 1] = 42; '
                                        'return values[count - 1]; } public static int Main() { return Use(8); }', 42),
    'variable-length array starts zeroed': (DIRTY + ' static int Read(int count) { int[count] values; int sum = 0; '
                                            'int i = 0; while (i < count) { sum = sum + values[i]; i = i + 1; } '
                                            'return sum; } public static int Main() { Dirty(); '
                                            'return Read(64) == 0 ? 42 : 1; }', 42),
    'stackalloc negative count': ('public static int Main() { int count = -1; int* value = stackalloc int[count]; '
                                  'return 0; }', 134),
    'stackalloc count over the stack reserve': ('public static int Main() { int count = 1048577; '
                                                'byte* value = stackalloc byte[count]; return 0; }', 134),
    'stackalloc constant over the stack reserve': ('public static int Main() { byte* value = stackalloc byte[1048577]; '
                                                   'return 0; }', 'does not fit on the stack'),
    'stackalloc zero count': ('public static int Main() { int* value = stackalloc int[0]; return 42; }', 42),
    'stackalloc in a loop': ('public static int Main() { int i = 0; int total = 0; while (i < 3) { '
                             'int* value = stackalloc int[4]; value[0] = i; total = total + value[0]; i = i + 1; } '
                             'return total == 3 ? 42 : 1; }', 42),
    'stackalloc starts zeroed': (DIRTY + ' static int Read() { int* values = stackalloc int[64]; int sum = 0; '
                                 'int i = 0; while (i < 64) { sum = sum + values[i]; i = i + 1; } return sum; } '
                                 'public static int Main() { Dirty(); return Read() == 0 ? 42 : 1; }', 42),
}

DELEGATES = {
    'null delegate invocation': ('delegate int Transform(int value); class Program { public static int Main() { '
                                 'Transform operation = null; return operation(42); } }', 135),
    'delegate invocation': ('delegate int Transform(int value); class Program { static int Read(int value) '
                            '{ return value; } public static int Main() { Transform operation = Read; '
                            'return operation(42); } }', 42),
}

PROJECT = ('<Project><PropertyGroup><Name>Limits</Name><OutputType>Exe</OutputType>'
           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" /></ItemGroup></Project>')


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=120)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    programs = {name: ('class Program { ' + body + ' }', expected) for name, (body, expected) in CASES.items()}
    programs.update(DELEGATES)
    with tempfile.TemporaryDirectory(prefix='nsc-array-stack-limits-') as temporary:
        root = Path(temporary)
        for index, (name, (source, expected)) in enumerate(programs.items()):
            folder = root / ('case-' + str(index))
            folder.mkdir()
            (folder / 'Program.ns').write_text(source, encoding='utf-8')
            project = folder / 'Case.nsproj'
            project.write_text(PROJECT, encoding='utf-8')
            if isinstance(expected, str):
                # Some limits are checked when the program is lowered, so the rejection uses a full build.
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
    print('array_and_stack_limits: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
