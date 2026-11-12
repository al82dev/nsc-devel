import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1])
    execution_timeout = float(os.environ.get('NSC_ASYNC_FINALLY_TIMEOUT', '120'))
    prefix = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Counter { public static int Log; }
class Program { public static async int Slow() { yield; return 7; }
 public static async int Immediate() { return 7; }
 BODY
}
'''
    cases = {
        'exception': '''public static async int Work() throws Exception {
 try { throw new Exception(); } finally { int value = await Slow(); Counter.Log = 1; }
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Counter.Log == 1 ? 42 : 1;
}''',
        'child_error_finally_suspend': '''public static async int Fail() throws Exception {
 yield; throw new Exception();
} public static async int Work() throws Exception {
 try { int value = await Fail(); } finally { int value = await Slow(); Counter.Log = 1; } return 0;
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Counter.Log == 1 ? 42 : 10 + caught * 2 + Counter.Log;
}''',
        'child_error_finally_ready': '''public static async int Fail() throws Exception {
 yield; throw new Exception();
} public static async int Work() throws Exception {
 try { int value = await Fail(); } finally { int value = await Immediate(); Counter.Log = 1; } return 0;
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Counter.Log == 1 ? 42 : 10 + caught * 2 + Counter.Log;
}''',
        'nested_exception': '''public static async int Work() throws Exception {
 try { try { throw new Exception(); } finally { int value = await Slow(); Counter.Log = 1; } }
 finally { Counter.Log = Counter.Log * 10 + 2; }
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Counter.Log == 12 ? 42 : 2;
}''',
        'normal': '''public static async int Main() {
 try { Counter.Log = 1; } finally { int value = await Slow(); Counter.Log = Counter.Log * 10 + 2; }
 return Counter.Log == 12 ? 42 : 3;
}''',
        'return': '''public static async int Work() {
 try { return 5; } finally { int value = await Slow(); Counter.Log = 1; }
} public static async int Main() { int result = await Work();
 return result == 5 && Counter.Log == 1 ? 42 : 4;
}''',
        'return_diagnostic': '''public static async int Work() {
 try { return 5; } finally { int value = await Slow(); Counter.Log = 1; }
} public static async int Main() { int result = await Work();
 if (Counter.Log != 1) return 30 + result;
 return result == 5 ? 42 : 20 + result;
}''',
        'exception_diagnostic': '''public static async int Work() throws Exception {
 try { throw new Exception(); } finally { int value = await Slow(); Counter.Log = 1; }
} public static async int Main() { int caught = 0;
 try { int result = await Work(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Counter.Log == 1 ? 42 : 10 + caught * 2 + Counter.Log;
}''',
        'return_finally_throws': '''public static async int Work() throws Exception {
 try { return 5; } finally { int value = await Slow(); Counter.Log = 1; throw new Exception(); }
} public static async int Main() { int caught = 0;
 try { int result = await Work(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Counter.Log == 1 ? 42 : 10 + caught * 2 + Counter.Log;
}''',
        'return_finally_throws_ready': '''public static async int Work() throws Exception {
 try { return 5; } finally { int value = await Immediate(); Counter.Log = 1; throw new Exception(); }
} public static async int Main() { int caught = 0;
 try { int result = await Work(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Counter.Log == 1 ? 42 : 10 + caught * 2 + Counter.Log;
}''',
        'break': '''public static async int Main() { int iterations = 0;
 while (iterations < 2) { iterations = iterations + 1;
   try { break; } finally { int value = await Slow(); Counter.Log = Counter.Log + 1; }
 }
 return iterations == 1 && Counter.Log == 1 ? 42 : 5;
}''',
        'break_diagnostic': '''public static async int Main() { int iterations = 0;
 while (iterations < 2) { iterations = iterations + 1;
   try { break; } finally { int value = await Slow(); Counter.Log = Counter.Log + 1; }
 }
 return iterations == 1 && Counter.Log == 1 ? 42 : 10 + iterations * 3 + Counter.Log;
}''',
        'break_trace': '''public static async int Main() { int iterations = 0;
 while (iterations < 2) { iterations = iterations + 1;
   try { break; } finally { int value = await Slow(); Counter.Log = Counter.Log * 10 + iterations; }
 }
 if (iterations == 1 && Counter.Log == 1) return 42;
 if (iterations == 2 && Counter.Log == 12) return 52;
 return 53;
}''',
        'continue': '''public static async int Main() { int iterations = 0;
 while (iterations < 2) { iterations = iterations + 1;
   try { continue; } finally { int value = await Slow(); Counter.Log = Counter.Log + 1; }
 }
 return iterations == 2 && Counter.Log == 2 ? 42 : 6;
}''',
        'nested_return': '''public static async int Work() {
 try { try { return 5; } finally { int value = await Slow(); Counter.Log = 1; } }
 finally { int value = await Slow(); Counter.Log = Counter.Log * 10 + 2; }
} public static async int Main() { int result = await Work();
 return result == 5 && Counter.Log == 12 ? 42 : 7;
}''',
        'nested_normal': '''public static async int Main() {
 try { try { Counter.Log = 1; } finally { int a = await Slow(); Counter.Log = Counter.Log * 10 + 2; } }
 finally { int b = await Slow(); Counter.Log = Counter.Log * 10 + 3; }
 return Counter.Log == 123 ? 42 : 9;
}''',
        'nested_return_diagnostic': '''public static async int Work() {
 try { try { return 5; } finally { int value = await Slow(); Counter.Log = 1; } }
 finally { int value = await Slow(); Counter.Log = Counter.Log * 10 + 2; }
} public static async int Main() { int result = await Work();
 if (result != 5 && Counter.Log != 12) return 10;
 if (result != 5) return 20 + result; if (Counter.Log != 12) return 12; return 42;
}''',
        'return_ready': '''public static async int Work() {
 try { return 5; } finally { int value = await Immediate(); Counter.Log = 1; }
} public static async int Main() { int result = await Work();
 return result == 5 && Counter.Log == 1 ? 42 : 13;
}''',
        'nested_return_ready': '''public static async int Work() {
 try { try { return 5; } finally { int a = await Immediate(); Counter.Log = 1; } }
 finally { int b = await Immediate(); Counter.Log = Counter.Log * 10 + 2; }
} public static async int Main() { int result = await Work();
 return result == 5 && Counter.Log == 12 ? 42 : 14;
}''',
        'exception_ready': '''public static async int Work() throws Exception {
 try { throw new Exception(); } finally { int value = await Immediate(); Counter.Log = 1; }
} public static async int Main() { int caught = 0;
 try { int result = await Work(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Counter.Log == 1 ? 42 : 15;
}''',
        'exception_after_await_control': '''public static async int Work() throws Exception {
 int value = await Slow(); throw new Exception();
} public static async int Main() { int caught = 0;
 try { int result = await Work(); } catch (Exception error) { caught = 1; }
 return caught == 1 ? 42 : 17;
}''',
        'return_after_await_control': '''public static async int Work() {
 int value = await Slow(); return 5;
} public static async int Main() { int result = await Work(); return result == 5 ? 42 : 18; }''',
        'break_ready': '''public static async int Main() { int iterations = 0;
 while (iterations < 2) { iterations = iterations + 1;
   try { break; } finally { int value = await Immediate(); Counter.Log = Counter.Log + 1; }
 }
 return iterations == 1 && Counter.Log == 1 ? 42 : 16;
}''',
        'two_awaits': '''public static async int Work() throws Exception {
 try { throw new Exception(); } finally {
   int a = await Slow(); Counter.Log = 1; int b = await Slow(); Counter.Log = Counter.Log * 10 + 2;
 }
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Counter.Log == 12 ? 42 : 8;
}''',
    }
    selected = os.environ.get('NSC_ASYNC_FINALLY_CASES')
    if selected:
        requested = set(selected.split(','))
        if requested.difference(cases):
            raise ValueError('unknown async finally case')
        cases = {name: body for name, body in cases.items() if name in requested}
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-async-finally-continuation-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        for name, body in cases.items():
            (root / 'Main.ns').write_text(prefix.replace('BODY', body), encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / (name + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-o', str(exe),
                           '-' + level, '--no-artifact-cache']
                build = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                if build.returncode != 0:
                    failures.append((name, level, 'build', build.stdout + build.stderr))
                    continue
                if os.name != 'nt':
                    print(f'{name} {level}: compiled; execution skipped', flush=True)
                    continue
                try:
                    result = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True,
                                            errors='replace', timeout=execution_timeout)
                except subprocess.TimeoutExpired:
                    print(f'{name} {level}: timeout after {execution_timeout:g}s', flush=True)
                    failures.append((name, level, 'timeout', execution_timeout))
                    continue
                print(f'{name} {level}: exit={result.returncode}', flush=True)
                if result.returncode != 42:
                    failures.append((name, level, 'run', result.returncode))
    if failures:
        for failure in failures:
            print(f'FAIL {failure}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
