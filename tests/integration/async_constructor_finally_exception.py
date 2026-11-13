"""Exercise constructor exceptions across a suspendible async finally."""

from pathlib import Path
import subprocess
import sys

from isolated_a_runner import absolute_a, setup


SOURCE = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public int Id; public Exception(int id) { Id = id; } } }
class Gate { static Gate() throws Exception { GATE } public static int Read() { return 1; } }
class ProbeValue { public static int Init = Gate.Read(); public int Id; public ProbeValue(int id) CTOR_THROWS { Id = id; CTOR_BODY } }
class Program { public static int Log; public static async int Slow() { SUSPEND return 1; }
 public static async int Work() throws Exception {
  try { ProbeValue value = new ProbeValue(4); return value.Id; }
  finally { int ignored = await Slow(); Log = Log + 7; }
 }
 public static async int Main() { int result = 0; try { result = await Work(); }
  catch (Exception error) { result = error.Id; }
  return result == EXPECTED && Log == 7 ? 42 : 1;
 }
}'''


def main():
    if len(sys.argv) != 3:
        raise SystemExit('expected nsc and a new evidence directory on A:')
    compiler, root = (absolute_a(value) for value in sys.argv[1:])
    repository = Path(__file__).resolve().parents[2]
    environment = setup(root, repository)
    failures = []
    for kind in ('static', 'constructor', 'success'):
        for suspension in ('ready', 'suspended'):
            source = SOURCE.replace('GATE', 'throw new Exception(9);' if kind == 'static' else '')
            source = source.replace('CTOR_THROWS', 'throws Exception' if kind == 'constructor' else '')
            source = source.replace('CTOR_BODY', 'throw new Exception(9);' if kind == 'constructor' else '')
            source = source.replace('SUSPEND', 'yield;' if suspension == 'suspended' else '')
            source = source.replace('EXPECTED', '4' if kind == 'success' else '9')
            case = root / (kind + '_' + suspension)
            case.mkdir()
            (case / 'Main.ns').write_text(source, encoding='utf-8')
            project = case / 'Probe.nsproj'
            project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                               '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = case / (level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                print('COMMAND', subprocess.list2cmdline(command), flush=True)
                build = subprocess.run(command, cwd=repository, env=environment, capture_output=True,
                                       text=True, errors='replace', timeout=120)
                (case / (level + '.log')).write_text(build.stdout + build.stderr, encoding='utf-8')
                if build.returncode:
                    failures.append((kind, suspension, level, 'compile', build.returncode))
                    continue
                run = subprocess.run([str(executable)], cwd=repository, env=environment,
                                     capture_output=True, text=True, errors='replace', timeout=10)
                print('RUN', kind, suspension, level, run.returncode, flush=True)
                if run.returncode != 42:
                    failures.append((kind, suspension, level, 'run', run.returncode))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
