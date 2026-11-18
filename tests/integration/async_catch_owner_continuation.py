"""Exercise exception ownership across coroutine suspension and cancellation."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


PREFIX = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public static int Drops; public static int Log;
 public int Id; public Exception(int id) { Id = id; }
 ~Exception() { Drops = Drops + 1; Log = Log * 10 + Id; } } }
class Counter { public static int Log; }
class Program { public static async int Slow() { yield; return 7; }
 BODY
}
'''

CASES = {
    'read_after_suspend': '''public static async int Work() throws Exception {
 try { throw new Exception(37); } catch (Exception error) { int value = await Slow(); return error.Id; }
} public static async int Main() { try { int value = await Work();
 return value == 37 && Exception.Drops == 1 ? 42 : 1; } catch (Exception error) { return 2; } }''',
    'rethrow_after_suspend': '''public static async int Work() throws Exception {
 try { throw new Exception(37); } catch (Exception error) { int value = await Slow(); throw; }
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) {
  caught = error.Id == 37 && Exception.Drops == 0 ? 1 : 2;
 }
 return caught == 1 && Exception.Drops == 1 ? 42 : 3; }''',
    'move_caught_before_await': '''public static async int Work() throws Exception {
 try { throw new Exception(37); } catch (Exception error) {
  owned Exception saved = move(error); int value = await Slow(); return saved.Id;
 }
} public static async int Main() { try { int value = await Work();
 return value == 37 && Exception.Drops == 1 ? 42 : 8; } catch (Exception error) { return 9; } }''',
    'replace_after_await': '''public static async int Work() throws Exception {
 try { throw new Exception(1); } catch (Exception error) {
  int value = await Slow(); throw new Exception(2);
 }
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) {
  caught = error.Id == 2 && Exception.Drops == 1 && Exception.Log == 1 ? 1 : 2;
 }
 return caught == 1 && Exception.Drops == 2 && Exception.Log == 12 ? 42 : 8; }''',
    'await_error_in_catch': '''public static async int Fail() throws Exception {
 yield; throw new Exception(2);
} public static async int Work() throws Exception {
 try { throw new Exception(1); } catch (Exception error) {
  int value = await Fail(); return value;
 }
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) {
  caught = error.Id == 2 && Exception.Drops == 1 && Exception.Log == 1 ? 1 : 2;
 }
 return caught == 1 && Exception.Drops == 2 && Exception.Log == 12 ? 42 : 10; }''',
    'await_error_inner_finally': '''public static async int Fail() throws Exception {
 yield; throw new Exception(2);
} public static async int Work() throws Exception {
 try { throw new Exception(1); } catch (Exception error) {
  try { int value = await Fail(); return value; }
  finally { if (Exception.Drops == 0) Counter.Log = error.Id; else Counter.Log = 99; }
 }
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) {
  caught = error.Id == 2 && Exception.Drops == 1 && Exception.Log == 1 ? 1 : 2;
 }
 return caught == 1 && Exception.Drops == 2 && Exception.Log == 12 && Counter.Log == 1 ? 42 : 11; }''',
    'await_error_local_catch': '''public static async int Fail() throws Exception {
 yield; throw new Exception(2);
} public static async int Work() throws Exception {
 int result = 0; try { throw new Exception(1); } catch (Exception outer) {
  try { int value = await Fail(); } catch (Exception inner) {
   if (outer.Id != 1 || inner.Id != 2 || Exception.Drops != 0) return 1;
   result = 7;
  }
  if (Exception.Drops != 1 || Exception.Log != 2 || outer.Id != 1) return 2;
  int value = await Slow(); result = result + value;
 } return result;
} public static async int Main() { try { int value = await Work();
 return value == 14 && Exception.Drops == 2 && Exception.Log == 21 ? 42 : 3;
} catch (Exception error) { return 4; } }''',
    'nested_catches': '''public static async int Work() throws Exception {
 int result = 0; try { throw new Exception(37); } catch (Exception outer) {
  int first = await Slow(); try { throw new Exception(5); } catch (Exception inner) {
   int second = await Slow(); result = outer.Id + inner.Id;
  }
  if (outer.Id != 37 || Exception.Drops != 1) return 1;
 } return result;
} public static async int Main() { try { int value = await Work();
 return value == 42 && Exception.Drops == 2 ? 42 : 4; } catch (Exception error) { return 5; } }''',
    'cancel_nested_catches': '''public static AsyncIterator<int, int> Gen() {
 try { try { throw new Exception(1); } catch (Exception outer) {
  try { try { throw new Exception(2); } catch (Exception inner) {
   try { int value = await Slow(); yield return outer.Id + inner.Id; }
   finally { Exception.Log = Exception.Log * 10 + 3; }
  } } finally { Exception.Log = Exception.Log * 10 + 4; }
 } } finally { Exception.Log = Exception.Log * 10 + 5; }
} public static async int Main() {
 AsyncIterator<int, int> iterator = Gen(); bool first = await iterator.MoveNextAsync();
 if (!first || iterator.Current != 3 || Exception.Log != 0) return 6;
 iterator.Cancel(); bool second = await iterator.MoveNextAsync();
 return !second && Exception.Drops == 2 && Exception.Log == 32415 ? 42 : 7;
}''',
}

CASES['drop_nested_catches'] = CASES['cancel_nested_catches'].replace(
    'iterator.Cancel(); bool second = await iterator.MoveNextAsync();\n'
    ' return !second && Exception.Drops == 2 && Exception.Log == 32415 ? 42 : 7;',
    'iterator.Drop();\n return Exception.Drops == 2 && Exception.Log == 32415 ? 42 : 7;')
assert 'iterator.Drop()' in CASES['drop_nested_catches']


def main():
    if os.name != 'nt':
        raise SystemExit('this directed test requires native Windows execution')
    compiler = Path(sys.argv[1]).resolve()
    repository = Path(__file__).resolve().parents[2]
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-async-catch-owner-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                           '</ItemGroup></Project>', encoding='utf-8')
        for name, body in CASES.items():
            (root / 'Main.ns').write_text(PREFIX.replace('BODY', body), encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                command = [compiler, '--emit-project-oop-exe', project, '-' + level,
                           '-o', executable, '--no-artifact-cache']
                result = subprocess.run([str(value) for value in command], cwd=repository,
                                        capture_output=True, text=True, errors='replace', timeout=120)
                if result.returncode != 0:
                    failures.append((name, level, 'compile', result.stdout + result.stderr))
                    continue
                result = subprocess.run([str(executable)], cwd=repository, capture_output=True,
                                        text=True, errors='replace', timeout=120)
                print(name, level, 'exit=' + str(result.returncode), flush=True)
                if result.returncode != 42:
                    failures.append((name, level, 'run', result.returncode))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
