import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1])
    prefix = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Counter { public static int Builds; public static int Drops; public static int After; }
struct Guard { public int Id; public Guard(int id) { Id = id; Counter.Builds = Counter.Builds * 10 + id; }
 ~Guard() { Counter.Drops = Counter.Drops * 10 + Id; } }
class Program {
 public static async int Slow() { yield; return 7; }
 public static async int SlowFail() throws Exception { yield; throw new Exception(); }
 public static int Later() { Counter.After = Counter.After + 1; return 9; }
 public static int Fail() throws Exception { throw new Exception(); }
 BODY
}
'''
    cases = {
        'two_awaits': '''public static async int Main() {
 { (Guard, int, Guard, int, int) pair = (new Guard(1), await Slow(), new Guard(2), await Slow(), Later()); }
 return Counter.Builds == 12 && Counter.Drops == 21 && Counter.After == 1 ? 42 : 1;
}''',
        'nested_await': '''public static async int Main() {
 { ((Guard, int), int) pair = ((new Guard(1), await Slow()), Later()); }
 return Counter.Builds == 1 && Counter.Drops == 1 && Counter.After == 1 ? 42 : 9;
}''',
        'repeated_await': '''public static async int Main() { int i = 0;
 while (i < 2) { (Guard, int) pair = (new Guard(1), await Slow()); i = i + 1; }
 return i == 2 && Counter.Builds == 11 && Counter.Drops == 11 ? 42 : 10;
}''',
        'throw_after_await': '''public static async int Main() { int caught = 0;
 try { (Guard, int, int) pair = (new Guard(1), await Slow(), Fail()); }
 catch (Exception error) { caught = 1; }
 return caught == 1 && Counter.Builds == 1 && Counter.Drops == 1 ? 42 : 2;
}''',
        'throw_during_await': '''public static async int Main() { int caught = 0;
 try { (Guard, int) pair = (new Guard(1), await SlowFail()); }
 catch (Exception error) { caught = 1; }
 return caught == 1 && Counter.Builds == 1 && Counter.Drops == 1 ? 42 : 3;
}''',
        'propagate_await_error': '''public static async int Work() throws Exception {
 (Guard, int) pair = (new Guard(1), await SlowFail()); return 0;
} public static async int Main() { int caught = 0;
 try { int unused = await Work(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Counter.Builds == 1 && Counter.Drops == 1 ? 42 : 4;
}''',
        'propagate_with_finally': '''public static async int Work() throws Exception {
 try { (Guard, int) pair = (new Guard(1), await SlowFail()); }
 finally { Counter.After = Counter.Drops * 10 + 3; } return 0;
} public static async int Main() { int caught = 0;
 try { int unused = await Work(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Counter.Builds == 1 && Counter.Drops == 1 && Counter.After == 13 ? 42 : 5;
}''',
        'await_in_finally': '''public static async int Work() throws Exception {
 try { Counter.After = 1; }
 finally { (Guard, int) pair = (new Guard(1), await SlowFail()); }
 return 0;
} public static async int Main() { int caught = 0;
 try { int unused = await Work(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Counter.Builds == 1 && Counter.Drops == 1 && Counter.After == 1 ? 42 : 6;
}''',
        'await_in_unwind_finally': '''public static async int Work() throws Exception {
 try { throw new Exception(); }
 finally { (Guard, int) pair = (new Guard(1), await Slow()); }
} public static async int Main() { int caught = 0;
 try { int unused = await Work(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Counter.Builds == 1 && Counter.Drops == 1 ? 42 : 7;
}''',
    }
    with tempfile.TemporaryDirectory(prefix='nsc-tuple-await-replay-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        for name, body in cases.items():
            if len(sys.argv) > 2 and name not in sys.argv[2:]:
                continue
            (root / 'Main.ns').write_text(prefix.replace('BODY', body), encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / (name + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-o', str(exe), '-' + level]
                build = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                assert build.returncode == 0, (name, level, build.stdout, build.stderr)
                if os.name == 'nt':
                    result = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                    print(f'{name} {level}: exit={result.returncode}', flush=True)
                    assert result.returncode == 42, (name, level, result.returncode, result.stdout, result.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
