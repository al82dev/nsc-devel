"""Check cancellation through finally for primitive async iterator items."""

from pathlib import Path
import subprocess
import sys

from isolated_a_runner import absolute_a, setup


SOURCE = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception {} }
class Program {
 public static int Log;
 public static AsyncIterator<ITEM, Exception> Gen() {
  try { yield return FIRST; } finally { Log = 1; yield return SECOND; Log = 12; }
 }
 public static async int Main() { try {
  AsyncIterator<ITEM, Exception> it = Gen(); if (!await it.MoveNextAsync()) return 20;
  it.Cancel(); if (await it.MoveNextAsync()) return 22;
  it.Drop(); return Log == 12 ? 42 : 23;
 } catch (Exception e) { return 29; } }
}'''


def main():
    if len(sys.argv) != 3:
        raise SystemExit('expected nsc and new evidence directory on A:')
    compiler, root = (absolute_a(value) for value in sys.argv[1:])
    repository = Path(__file__).resolve().parents[2]
    environment = setup(root, repository)
    failures = []
    for typename, first, second in (('bool', 'true', 'false'), ('double', '1.5', '2.5'),
                                    ('long', '1l', '2l'), ('uint', '1u', '2u')):
        case = root / typename
        case.mkdir()
        (case / 'Main.ns').write_text(SOURCE.replace('ITEM', typename).replace('FIRST', first)
                                      .replace('SECOND', second), encoding='utf-8')
        project = case / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = case / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            print('CWD', repository, 'COMMAND', subprocess.list2cmdline(command), flush=True)
            result = subprocess.run(command, cwd=repository, env=environment, capture_output=True,
                                    text=True, errors='replace', timeout=120)
            if result.returncode:
                failures.append((typename, level, 'compile', result.stdout + result.stderr))
                continue
            result = subprocess.run([str(executable)], cwd=repository, env=environment, capture_output=True,
                                    text=True, errors='replace', timeout=10)
            print('RUN', typename, level, result.returncode, flush=True)
            if result.returncode != 42:
                failures.append((typename, level, 'run', result.returncode))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
