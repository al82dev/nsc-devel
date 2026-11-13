"""Exercise source-visible errors around awaited synchronous future factories."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


PRELUDE = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public int Id;
 public Exception(int id) { Id = id; } } }
'''
CASES = {
    'async_direct': '''class Program {
 public static async int Fail() throws Exception { yield; throw new Exception(42); }
 public static async int Main() { try { int x = await Fail(); return x; }
 catch (Exception error) { return error.Id; } } }''',
    'typed_future_factory': '''class Program {
 public static async int Fail() throws Exception { yield; throw new Exception(42); }
 public static Future<int, Exception> Make() { return Fail(); }
 public static async int Main() { try { int x = await Make(); return x; }
 catch (Exception error) { return error.Id; } } }''',
    'static_factory': '''class Program {
 public static Future<int> Make() throws Exception { throw new Exception(42); }
 public static async int Main() { try { int x = await Make(); return x; }
 catch (Exception error) { return error.Id; } } }''',
    'namespace_factory': '''namespace Helpers {
 public Future<int> Make() throws Exception { throw new Exception(42); } }
 class Program { public static async int Main() { try { int x = await Helpers.Make(); return x; }
 catch (Exception error) { return error.Id; } } }''',
    'virtual_factory': '''class Base { public virtual Future<int> Make() throws Exception {
 throw new Exception(10); } }
 class Derived : Base { public override Future<int> Make() throws Exception { throw new Exception(42); } }
 class Program { public static async int Main() { Base instance = new Derived();
 try { int x = await instance.Make(); return x; } catch (Exception error) { return error.Id; } } }''',
}
TYPE_NEGATIVE = '''class Program {
 public static async int Fail() throws Exception { yield; throw new Exception(42); }
 public static Future<int> Make() { return Fail(); }
 public static int Main() { return 42; } }'''


def main():
    if os.name != 'nt':
        return 77
    compiler = Path(sys.argv[1]).resolve()
    repo = Path(__file__).resolve().parents[2]
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-future-factory-errors-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        cases = dict(CASES)
        cases['typed_return_rejected'] = TYPE_NEGATIVE
        for name, source in cases.items():
            (root / 'Main.ns').write_text(PRELUDE + source, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                compiled = subprocess.run(command, cwd=repo, capture_output=True,
                                          text=True, errors='replace', timeout=120)
                if name == 'typed_return_rejected':
                    diagnostic = compiled.stdout + compiled.stderr
                    if compiled.returncode == 0 or 'cannot convert return expression' not in diagnostic:
                        failures.append((name, level, 'expected type rejection', diagnostic))
                    else:
                        print(name, level, 'expected diagnostic', flush=True)
                    continue
                if compiled.returncode:
                    failures.append((name, level, 'compile', compiled.stdout + compiled.stderr))
                    continue
                result = subprocess.run([str(executable)], cwd=repo, capture_output=True,
                                        text=True, errors='replace', timeout=30)
                print(name, level, 'exit=' + str(result.returncode), flush=True)
                if result.returncode != 42:
                    failures.append((name, level, 'run', result.returncode))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
