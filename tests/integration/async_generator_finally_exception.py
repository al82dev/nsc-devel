"""Exercise exception ownership while an async generator yields from finally."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

PREFIX = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public static int Builds; public static int Drops; public static int Log;
 public int Id; public Exception(int id) { Id = id; Builds = Builds + 1; }
 ~Exception() { Drops = Drops + 1; Log = Log * 10 + Id; } } }
class Counter { public static int Log; }
class Program { public static async int Slow() { yield; return 7; }
 BODY
}
'''

CASES = {'drop_finally_yield': '''public static AsyncIterator<int, Exception> Gen() throws Exception {
 try { throw new Exception(1); } finally { yield return 5; }
} public static async int Main() { int observed = 0;
 { AsyncIterator<int, Exception> iterator = Gen();
 try { bool first = await iterator.MoveNextAsync(); if (first && iterator.Current == 5) observed = 1;
  iterator.Drop();
 } catch (Exception error) { observed = 2; } }
 if (observed != 1) return 10 + observed;
 if (Exception.Builds != 1) return 60 + Exception.Builds;
 if (Exception.Drops != 1) return 50 + Exception.Drops;
 if (Exception.Log != 1) return 70 + Exception.Log; return 42; }''',
 'resume_finally_yield': '''public static AsyncIterator<int, Exception> Gen() throws Exception {
 try { throw new Exception(1); } finally { yield return 5; }
} public static async int Main() { int caught = 0;
 AsyncIterator<int, Exception> iterator = Gen();
 try { bool first = await iterator.MoveNextAsync();
  if (!first || iterator.Current != 5) return 20;
  bool second = await iterator.MoveNextAsync();
 } catch (Exception error) { caught = error.Id; }
 if (caught != 1) return 30 + caught;
 if (Exception.Builds != 1) return 60 + Exception.Builds;
 if (Exception.Drops != 1) return 50 + Exception.Drops;
 return Exception.Log == 1 ? 42 : 70 + Exception.Log; }''',
 'cancel_finally_yield': '''public static AsyncIterator<int, Exception> Gen() throws Exception {
 try { throw new Exception(1); } finally { yield return 5; }
} public static async int Main() { try { { AsyncIterator<int, Exception> iterator = Gen();
 bool first = await iterator.MoveNextAsync(); if (!first || iterator.Current != 5) return 20;
 iterator.Cancel(); if (Exception.Drops != 0) return 21;
 bool second = await iterator.MoveNextAsync(); if (second) return 22; }
 return Exception.Builds == 1 && Exception.Drops == 1 && Exception.Log == 1 ? 42 : 23;
 } catch (Exception error) { return 29; } }''',
 'two_yields_exception': '''public static AsyncIterator<int, Exception> Gen() throws Exception {
 try { throw new Exception(1); } finally { yield return 5; yield return 6; }
} public static async int Main() { int caught = 0; AsyncIterator<int, Exception> iterator = Gen();
 try { bool first = await iterator.MoveNextAsync(); if (!first || iterator.Current != 5) return 20;
  bool second = await iterator.MoveNextAsync(); if (!second || iterator.Current != 6) return 21;
  bool third = await iterator.MoveNextAsync();
 } catch (Exception error) { caught = error.Id; }
 return caught == 1 && Exception.Builds == 1 && Exception.Drops == 1 && Exception.Log == 1 ? 42 : 22; }''',
 'nested_yields_exception': '''public static AsyncIterator<int, Exception> Gen() throws Exception {
 try { try { throw new Exception(1); } finally { yield return 5; } } finally { yield return 6; }
} public static async int Main() { int caught = 0; AsyncIterator<int, Exception> iterator = Gen();
 try { bool first = await iterator.MoveNextAsync(); if (!first || iterator.Current != 5) return 20;
  bool second = await iterator.MoveNextAsync(); if (!second || iterator.Current != 6) return 21;
  bool third = await iterator.MoveNextAsync();
 } catch (Exception error) { caught = error.Id; }
 return caught == 1 && Exception.Builds == 1 && Exception.Drops == 1 && Exception.Log == 1 ? 42 : 22; }''',
 'await_origin_drop': '''public static async int Fail() throws Exception { yield; throw new Exception(1); }
public static AsyncIterator<int, Exception> Gen() throws Exception {
 try { int value = await Fail(); } finally { yield return 5; }
} public static async int Main() { try { { AsyncIterator<int, Exception> iterator = Gen();
 bool first = await iterator.MoveNextAsync(); if (!first || iterator.Current != 5) return 20;
 iterator.Drop(); }
 return Exception.Builds == 1 && Exception.Drops == 1 && Exception.Log == 1 ? 42 : 21;
 } catch (Exception error) { return 29; } }''',
 'await_origin_resume': '''public static async int Fail() throws Exception { yield; throw new Exception(1); }
public static AsyncIterator<int, Exception> Gen() throws Exception {
 try { int value = await Fail(); } finally { yield return 5; }
} public static async int Main() { int caught = 0; { AsyncIterator<int, Exception> iterator = Gen();
 try { bool first = await iterator.MoveNextAsync(); if (!first || iterator.Current != 5) return 20;
  bool second = await iterator.MoveNextAsync();
 } catch (Exception error) { caught = error.Id; } }
 return caught == 1 && Exception.Builds == 1 && Exception.Drops == 1 && Exception.Log == 1 ? 42 : 22; }''',
 'normal_yields': '''public static AsyncIterator<int, Exception> Gen() {
 try { yield return 1; } finally { yield return 5; }
} public static async int Main() { try { AsyncIterator<int, Exception> iterator = Gen();
 bool first = await iterator.MoveNextAsync(); if (!first || iterator.Current != 1) return 20;
 bool second = await iterator.MoveNextAsync(); if (!second || iterator.Current != 5) return 21;
 bool third = await iterator.MoveNextAsync(); return !third && Exception.Drops == 0 ? 42 : 22;
 } catch (Exception error) { return 29; } }''',
    'replace_by_child': '''public static async int Fail() throws Exception { yield; throw new Exception(2); }
public static AsyncIterator<int, Exception> Gen() throws Exception {
 try { throw new Exception(1); } finally { yield return 5; int value = await Fail(); }
} public static async int Main() { int caught = 0; { AsyncIterator<int, Exception> iterator = Gen();
 try { bool first = await iterator.MoveNextAsync(); if (!first || iterator.Current != 5) return 20;
  bool second = await iterator.MoveNextAsync();
 } catch (Exception error) { caught = error.Id; } }
 return caught == 2 && Exception.Builds == 2 && Exception.Drops == 2 && Exception.Log == 12 ? 42 : 22; }''',
}

def main():
    if os.name != 'nt':
        print('this directed test requires native Windows execution')
        return 77
    compiler = Path(sys.argv[1]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-finally-pending-') as temporary:
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
                result = subprocess.run([str(value) for value in command], cwd=root,
                                        capture_output=True, text=True, errors='replace', timeout=120)
                if result.returncode != 0:
                    failures.append((name, level, 'compile', result.stdout + result.stderr))
                    continue
                try:
                    result = subprocess.run([str(executable)], cwd=root, capture_output=True,
                                            text=True, errors='replace', timeout=10)
                except subprocess.TimeoutExpired:
                    print(name, level, 'timeout=10s', flush=True)
                    failures.append((name, level, 'timeout', 10))
                    continue
                print(name, level, 'exit=' + str(result.returncode), flush=True)
                if result.returncode != 42:
                    failures.append((name, level, 'run', result.returncode))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
