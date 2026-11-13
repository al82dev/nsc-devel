"""Exercise constructor exceptions across a suspendible async finally."""

from pathlib import Path
import subprocess
import sys

from isolated_a_runner import absolute_a, setup


SOURCE = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public static int Drops; public int Id; public Exception(int id) { Id = id; } ~Exception() { Drops = Drops + 1; } } }
class Gate { static Gate() throws Exception { GATE } public static int Read() { return 1; } }
class Resource { public static int Drops; ~Resource() { Drops = Drops + 1; } }
class ProbeValue { public static int Init = Gate.Read(); public int Id; OWNED_FIELD public ProbeValue(int id) CTOR_THROWS { Id = id; CTOR_BODY } }
class Program { public static int Log; public static async int Slow() { SUSPEND return 1; }
 public static async int Work() throws Exception {
  try { ProbeValue value = new ProbeValue(4); return value.Id; }
  finally { int ignored = await Slow(); Log = Log + 7; }
 }
 public static async int Main() { int result = 0; try { result = await Work(); }
  catch (Exception error) { result = error.Id; }
  return result == EXPECTED && Log == 7 && Exception.Drops == ERROR_DROPS && Resource.Drops == RESOURCE_DROPS ? 42 : 1;
 }
}'''


def main():
    if len(sys.argv) not in (3, 4):
        raise SystemExit('expected nsc, a new evidence directory on A:, and optionally Core.lib')
    compiler, root = (absolute_a(value) for value in sys.argv[1:3])
    core = absolute_a(sys.argv[3]) if len(sys.argv) == 4 else None
    metadata = core.with_suffix('.nsmeta') if core else None
    if core and (not core.is_file() or not metadata.is_file()):
        raise SystemExit('Core library and metadata must exist')
    repository = Path(__file__).resolve().parents[2]
    environment = setup(root, repository)
    failures = []
    for kind in ('static', 'constructor', 'owned_constructor', 'success'):
        for suspension in ('ready', 'suspended'):
            source = SOURCE.replace('GATE', 'throw new Exception(9);' if kind == 'static' else '')
            source = source.replace('CTOR_THROWS', 'throws Exception' if kind in ('constructor', 'owned_constructor') else '')
            source = source.replace('OWNED_FIELD', 'public owned Resource resource;' if kind == 'owned_constructor' else '')
            body = 'resource = new Resource(); throw new Exception(9);' if kind == 'owned_constructor' else (
                'throw new Exception(9);' if kind == 'constructor' else '')
            source = source.replace('CTOR_BODY', body)
            source = source.replace('SUSPEND', 'yield;' if suspension == 'suspended' else '')
            source = source.replace('EXPECTED', '4' if kind == 'success' else '9')
            source = source.replace('ERROR_DROPS', '0' if kind == 'success' else '1')
            source = source.replace('RESOURCE_DROPS', '1' if kind == 'owned_constructor' else '0')
            if core:
                source = source.replace(
                    'namespace NativeSharp.Core { public class Exception { public static int Drops; public int Id; public Exception(int id) { Id = id; } ~Exception() { Drops = Drops + 1; } } }\n', '')
                source = source.replace('new Exception(9)', 'new Exception()')
                source = source.replace('result = error.Id;', 'result = 9;')
                source = source.replace(' && Exception.Drops == 0', '').replace(' && Exception.Drops == 1', '')
            case = root / (kind + '_' + suspension)
            case.mkdir()
            (case / 'Main.ns').write_text(source, encoding='utf-8')
            project = case / 'Probe.nsproj'
            library = ('<LibraryReference Include="' + core.as_posix() + '" Metadata="' +
                       metadata.as_posix() + '"/>' if core else '')
            project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                               '<ItemGroup><Compile Include="Main.ns"/>' + library +
                               '</ItemGroup></Project>', encoding='utf-8')
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
