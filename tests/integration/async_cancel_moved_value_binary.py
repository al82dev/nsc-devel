"""Run moved-value async cleanup with an independently built Core library."""

from pathlib import Path
import subprocess
import sys

from async_cancel_value_cleanup import EXCEPTION_MOVED_VALUE, LATE_OWNED_MOVE
from isolated_a_runner import absolute_a, setup


def main():
    if len(sys.argv) != 4:
        raise SystemExit('expected nsc, Core.lib and new evidence directory on A:')
    compiler, core, root = (absolute_a(value) for value in sys.argv[1:])
    metadata = core.with_suffix('.nsmeta')
    if not core.is_file() or not metadata.is_file():
        raise SystemExit('Core library and metadata must exist')
    repository = Path(__file__).resolve().parents[2]
    environment = setup(root, repository)
    sources = {
        'cancel': LATE_OWNED_MOVE.replace('namespace NativeSharp.Core { public class Exception { } }\n', ''),
        'exception': EXCEPTION_MOVED_VALUE.replace(
            'namespace NativeSharp.Core { public class Exception { public static int Drops; ~Exception() { Drops = Drops + 1; } } }\n',
            '').replace(' && Exception.Drops == 1', ''),
    }
    failures = []
    for name, source in sources.items():
        case = root / name
        case.mkdir()
        (case / 'Main.ns').write_text(source, encoding='utf-8')
        project = case / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
            '<Compile Include="Main.ns"/><LibraryReference Include="' + core.as_posix() +
            '" Metadata="' + metadata.as_posix() + '"/></ItemGroup></Project>', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = case / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            print('COMMAND', subprocess.list2cmdline(command), flush=True)
            build = subprocess.run(command, cwd=repository, env=environment, capture_output=True,
                                   text=True, errors='replace', timeout=120)
            if build.returncode:
                failures.append((name, level, 'compile', build.stdout + build.stderr))
                continue
            run = subprocess.run([str(executable)], cwd=repository, env=environment,
                                 capture_output=True, text=True, errors='replace', timeout=10)
            print('RUN', name, level, run.returncode, flush=True)
            if run.returncode != 42:
                failures.append((name, level, 'run', run.returncode))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
