"""Call and construction shapes with 255, 256 and 300 operands (LKF-509).

The expression machine kept the operand index of several frames in eight bits: struct and class
construction, allocator construction, aggregate initialization, stackalloc initializers, tuple
deconstruction, parameter packs and three core calls wrapped at 256 operands (class construction with 256
arguments never finished compiling). 1fd4d284e widened only
the direct static call; the phase is size_t in every frame now. Each program evaluates its operands in
order and checks the first and the last one, at O0 and O2.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

PRELUDE = ('public class Counter { public static int Calls = 0; '
           'public static int Next() { Calls = Calls + 1; return Calls; } }\n')


def shapes(count):
    parameters = ', '.join('int a' + str(i) for i in range(count))
    arguments = ', '.join('Counter.Next()' for _ in range(count))
    check = ('if (a0 != 1 || a' + str(count - 1) + ' != ' + str(count) + ' || Counter.Calls != ' + str(count) +
             ') { Ok = false; }')
    last = str(count - 1)
    yield 'instance call', (PRELUDE + 'public class Box { public bool Ok = true; public void F(' + parameters + ') { '
                            + check + ' } }\npublic class Program { public static int Main() { raii Box b = new Box(); '
                            'b.F(' + arguments + '); return b.Ok ? 42 : 1; } }')
    yield 'virtual call', (PRELUDE + 'public class Base { public bool Ok = true; public virtual void F(' + parameters +
                           ') { Ok = false; } }\npublic class Derived : Base { public override void F(' + parameters +
                           ') { ' + check + ' } }\npublic class Program { public static int Main() { '
                           'raii Base b = new Derived(); b.F(' + arguments + '); return b.Ok ? 42 : 1; } }')
    yield 'class construction', (PRELUDE + 'public class Box { public bool Ok = true; public Box(' + parameters +
                                 ') { ' + check + ' } }\npublic class Program { public static int Main() { '
                                 'raii Box b = new Box(' + arguments + '); return b.Ok ? 42 : 1; } }')
    yield 'struct construction', (PRELUDE + 'public struct Pair { public bool Ok; public Pair(' + parameters +
                                  ') { Ok = true; ' + check + ' } }\npublic class Program { public static int Main() {'
                                  ' Pair p = new Pair(' + arguments + '); return p.Ok ? 42 : 1; } }')
    yield 'delegate invocation', (PRELUDE + 'public delegate bool Wide(' + parameters + ');\npublic class Program { '
                                  'public static bool Ok = true; public static bool F(' + parameters + ') { ' + check +
                                  ' return Ok; } public static int Main() { Wide w = F; return w(' + arguments +
                                  ') ? 42 : 1; } }')
    values = ', '.join(str(i + 1) for i in range(count))
    yield 'array initializer', ('public class Program { public static int Main() { int[] a = new int[] { ' + values +
                                ' }; int r = a.Length == ' + str(count) + ' && a[' + last + '] == ' + str(count) +
                                ' ? 42 : 1; delete[] a; return r; } }')


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=120)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-large-arguments-') as temporary:
        root = Path(temporary)
        index = 0
        for count in (255, 256, 300):
            for name, source in shapes(count):
                index += 1
                folder = root / ('case-' + str(index))
                folder.mkdir()
                (folder / 'Program.ns').write_text(source, encoding='utf-8')
                project = folder / 'Case.nsproj'
                project.write_text('<Project><PropertyGroup><Name>Case</Name><OutputType>Exe</OutputType>'
                                   '</PropertyGroup><ItemGroup><Compile Include="Program.ns"/></ItemGroup></Project>',
                                   encoding='utf-8')
                for level in ('O0', 'O2'):
                    exe = folder / (level + '.exe')
                    build = run([compiler, '-' + level, '--emit-project-oop-exe', str(project), '-o', str(exe),
                                 '--no-artifact-cache', '--no-error-log'], folder)
                    if build.returncode:
                        failures.append((name, count, level, 'build', (build.stdout + build.stderr)[-300:]))
                        continue
                    executed = run([str(exe)], folder)
                    print(name, count, level, 'exit', executed.returncode, flush=True)
                    if executed.returncode != 42:
                        failures.append((name, count, level, 'exit', executed.returncode))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('large_argument_lists: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
