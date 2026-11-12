"""Exercise ownership when an awaited future fails during an exceptional finally."""
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
    'abandon_unstarted': '''public static async int Fail() throws Exception { yield; throw new Exception(2); }
public static async int Work() throws Exception {
 try { throw new Exception(1); } finally { int value = await Fail(); }
} public static int Main() { { var future = Work(); }
 return Exception.Drops == 0 && Exception.Log == 0 ? 42 : 9; }''',
    'child_suspends': '''public static async int Fail() throws Exception { yield; throw new Exception(2); }
public static async int Work() throws Exception {
 try { throw new Exception(1); } finally { int value = await Fail(); }
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) { caught = error.Id; }
 return caught == 2 && Exception.Drops == 2 && Exception.Log == 12 ? 42 : 8; }''',
    'child_ready': '''public static async int Fail() throws Exception { throw new Exception(2); }
public static async int Work() throws Exception {
 try { throw new Exception(1); } finally { int value = await Fail(); }
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) { caught = error.Id; }
 return caught == 2 && Exception.Drops == 2 && Exception.Log == 12 ? 42 : 8; }''',
    'preserve': '''public static async int Work() throws Exception {
 try { throw new Exception(1); } finally { int value = await Slow(); Counter.Log = value; }
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) { caught = error.Id; }
 return caught == 1 && Counter.Log == 7 && Exception.Drops == 1 && Exception.Log == 1 ? 42 : 5; }''',
    'nested': '''public static async int Fail() throws Exception { yield; throw new Exception(2); }
public static async int Work() throws Exception {
 try { try { throw new Exception(1); } finally { int value = await Fail(); } }
 finally { int value = await Slow(); Counter.Log = value; }
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) { caught = error.Id; }
 return caught == 2 && Counter.Log == 7 && Exception.Drops == 2 && Exception.Log == 12 ? 42 : 7; }''',
    'later_child_replaces': '''public static async int Fail() throws Exception { yield; throw new Exception(2); }
public static async int Work() throws Exception {
 try { throw new Exception(1); } finally { int first = await Slow(); Counter.Log = first; int second = await Fail(); }
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) { caught = error.Id; }
 return caught == 2 && Counter.Log == 7 && Exception.Drops == 2 && Exception.Log == 12 ? 42 : 6; }''',
    'original_from_child': '''public static async int Fail1() throws Exception { yield; throw new Exception(1); }
public static async int Fail2() throws Exception { yield; throw new Exception(2); }
public static async int Work() throws Exception {
 try { int value = await Fail1(); } finally { int value = await Fail2(); } return 0;
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) { caught = error.Id; }
 return caught == 2 && Exception.Drops == 2 && Exception.Log == 12 ? 42 : 10; }''',
    'original_child_preserved': '''public static async int Fail1() throws Exception { yield; throw new Exception(1); }
public static async int Work() throws Exception {
 try { int value = await Fail1(); } finally { int value = await Slow(); Counter.Log = value; } return 0;
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) { caught = error.Id; }
 return caught == 1 && Counter.Log == 7 && Exception.Drops == 1 && Exception.Log == 1 ? 42 : 11; }''',
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
                result = subprocess.run([str(executable)], cwd=root, capture_output=True,
                                        text=True, errors='replace', timeout=120)
                print(name, level, 'exit=' + str(result.returncode), flush=True)
                if result.returncode != 42:
                    failures.append((name, level, 'run', result.returncode))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
