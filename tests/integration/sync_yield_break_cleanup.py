import os
from pathlib import Path
import subprocess
import sys
import tempfile


CASES = {
    'catch_finally': '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public static int Drops; ~Exception() { Drops = Drops + 1; } } }
class Trace { public static int Value; }
class Program { public static Iterator<int> Gen() throws Exception {
try { throw new Exception(); } catch (Exception error) { yield break; }
finally { Trace.Value = Exception.Drops; } }
public static int Main() { try { Iterator<int> it = Gen(); if (it.MoveNext()) return 1;
it.Drop(); return Exception.Drops == 1 && Trace.Value == 1 ? 42 : 2;
} catch (Exception error) { return 3; } } }''',
    'nested_catch_finally': '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public static int Drops; ~Exception() { Drops = Drops + 1; } } }
class Trace { public static int Value; }
class Program { public static Iterator<int> Gen() throws Exception {
try { throw new Exception(); } catch (Exception outer) {
try { try { throw new Exception(); } catch (Exception inner) { yield break; } }
finally { Trace.Value = Trace.Value * 10 + Exception.Drops; } }
finally { Trace.Value = Trace.Value * 10 + Exception.Drops; } }
public static int Main() { try { Iterator<int> it = Gen(); if (it.MoveNext()) return 1;
it.Drop(); return Exception.Drops == 2 && Trace.Value == 12 ? 42 : 2;
} catch (Exception error) { return 3; } } }''',
    'normal_conditional': '''class Trace { public static int Value; }
class Guard { public static int Drops; ~Guard() { Drops = Drops + 1; } }
class Program { public static Iterator<int> Gen(bool stop) { owned Guard guard = new Guard();
try { if (stop) yield break; yield return 7; } finally { Trace.Value = Trace.Value + 1; } }
public static int Main() {
{ Iterator<int> first = Gen(true); if (first.MoveNext()) return 1;
first.Drop(); if (Trace.Value != 1 || Guard.Drops != 1) return 2; }
{ Iterator<int> second = Gen(false); if (!second.MoveNext() || second.Current != 7) return 3;
if (second.MoveNext()) return 4; second.Drop(); }
return Trace.Value == 2 && Guard.Drops == 2 ? 42 : 5; } }''',
}


def main():
    if os.name != 'nt':
        return 77
    compiler = Path(sys.argv[1]).resolve()
    repository = Path(__file__).resolve().parents[2]
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-sync-yield-break-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        for name, source in CASES.items():
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                print('COMMAND', subprocess.list2cmdline(command), 'CWD', repository, flush=True)
                build = subprocess.run(command, cwd=repository, capture_output=True, text=True,
                                       errors='replace', timeout=120)
                if build.returncode:
                    failures.append((name, level, 'compile', build.returncode, build.stdout + build.stderr))
                    continue
                try:
                    run = subprocess.run([str(executable)], cwd=repository, capture_output=True,
                                         text=True, errors='replace', timeout=10)
                except subprocess.TimeoutExpired:
                    failures.append((name, level, 'timeout'))
                    continue
                print(name, level, 'exit=' + str(run.returncode), flush=True)
                if run.returncode != 42:
                    failures.append((name, level, 'run', run.returncode, run.stdout + run.stderr))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
