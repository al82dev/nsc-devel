import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1])
    prefix = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Counter { public static int Order; }
struct Guard { public int Id; public Guard(int id) { Id = id; }
 ~Guard() { Counter.Order = Counter.Order * 10 + Id; } }
class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 42; } }
class Control { public static int Fail() throws Exception { throw new Exception(); }
 public static int Handled() { try { return Fail(); } catch (Exception error) { return 7; } } }
struct FailingGuard { public int Id; public FailingGuard(int id) { Id = Gate.Read(); }
 ~FailingGuard() { Counter.Order = Counter.Order * 10 + 9; } }
class Program { public static int Consume((Guard, Guard, int) pair) { return 0; }
 public static void Build() throws Exception {
   (Guard, Guard, int) pair = (new Guard(1), new Guard(2), Control.Fail()); }
 public static int Main() { int caught = 0;
 try { STATEMENT } catch (Exception error) { caught = 1; }
 if (caught != EXPECTED_CATCH) return 99;
 return Counter.Order == EXPECTED_ORDER ? 42 : Counter.Order; } }
'''
    cases = {
        'first': ('(int, Guard) pair = (Gate.Read(), new Guard(1));', 1, 0),
        'second': ('(Guard, int) pair = (new Guard(1), Gate.Read());', 1, 1),
        'third': ('(Guard, Guard, int) pair = (new Guard(1), new Guard(2), Gate.Read());', 1, 21),
        'aggregate': ('(Guard, Guard, int) pair = { new Guard(1), new Guard(2), Gate.Read() };', 1, 21),
        'nested': ('((Guard, Guard), int) pair = ((new Guard(1), new Guard(2)), Gate.Read());', 1, 21),
        'nested_partial': ('(Guard, (Guard, int)) pair = (new Guard(1), (new Guard(2), Gate.Read()));', 1, 21),
        'constructor_failure': ('(Guard, FailingGuard) pair = (new Guard(1), new FailingGuard(2));', 1, 1),
        'argument': ('int result = Consume((new Guard(1), new Guard(2), Gate.Read()));', 1, 21),
        'success': ('(Guard, Guard, int) pair = (new Guard(1), new Guard(2), 42);', 0, 21),
        'argument_success': ('int result = Consume((new Guard(1), new Guard(2), 42));', 0, 21),
        'handled_inside': ('(Guard, int) pair = (new Guard(1), Control.Handled());', 0, 1),
        'repeat_caught': ('''int seen = 0; int i = 0;
            while (i < 2) { try { (Guard, Guard, int) pair = (new Guard(1), new Guard(2), Control.Fail()); }
            catch (Exception inner) { seen = seen + 1; } i = i + 1; }
            if (seen != 2) return 98;''', 0, 2121),
        'caller_catch': ('Build();', 1, 21),
        'finally_order': ('''try { (Guard, Guard, int) pair = (new Guard(1), new Guard(2), Control.Fail()); }
            finally { Counter.Order = Counter.Order * 10 + 3; }''', 1, 213),
    }
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-tuple-partial-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        for name, (statement, caught, order) in cases.items():
            source = prefix.replace('STATEMENT', statement).replace('EXPECTED_CATCH', str(caught)) \
                           .replace('EXPECTED_ORDER', str(order))
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / (name + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-o', str(exe), '-' + level]
                build = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                if build.returncode != 0:
                    failures.append((name, level, 'build', build.stdout + build.stderr))
                    continue
                if os.name != 'nt':
                    print(f'{name} {level}: compiled; execution skipped', flush=True)
                    continue
                result = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
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
