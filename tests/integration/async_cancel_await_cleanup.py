"""Check cancellation through a finally whose awaited child suspends."""

from pathlib import Path
import subprocess
import sys

from isolated_a_runner import absolute_a, setup


PREFIX = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception {} }
class Program {
 public static int Log; public static int Calls;
 public static async int Slow() { Calls = Calls + 1; Log = Log * 10 + 2;
  yield; Log = Log * 10 + 3; return 4; }
 BODY
}'''

GENERATOR = '''public static AsyncIterator<int, Exception> Gen() {
 try { yield return 1; } finally { Log = Log * 10 + 1;
  int n = await Slow(); Log = Log * 10 + n; } }
'''

CASES = {
    'cancel_once': GENERATOR + '''public static async int Main() { try {
 AsyncIterator<int, Exception> it = Gen();
 if (!await it.MoveNextAsync() || it.Current != 1) return 20;
 it.Cancel(); if (Log != 0) return 21;
 if (await it.MoveNextAsync()) return 22;
 if (Log != 1234 || Calls != 1) return 23;
 if (await it.MoveNextAsync()) return 24;
 it.Drop(); return Log == 1234 && Calls == 1 ? 42 : 25;
 } catch (Exception e) { return 29; } }''',
    'cancel_repeated': GENERATOR + '''public static async int Main() { try {
 AsyncIterator<int, Exception> it = Gen();
 if (!await it.MoveNextAsync()) return 20;
 it.Cancel(); it.Cancel(); if (Log != 0) return 21;
 if (await it.MoveNextAsync()) return 22;
 if (Log != 1234 || Calls != 1) return 23;
 it.Cancel(); it.Drop(); return Log == 1234 && Calls == 1 ? 42 : 25;
 } catch (Exception e) { return 29; } }''',
    'normal': GENERATOR + '''public static async int Main() { try {
 AsyncIterator<int, Exception> it = Gen();
 if (!await it.MoveNextAsync() || it.Current != 1) return 20;
 if (await it.MoveNextAsync()) return 22;
 it.Drop(); return Log == 1234 && Calls == 1 ? 42 : 23;
 } catch (Exception e) { return 29; } }''',
}


def main():
    if len(sys.argv) != 3:
        raise SystemExit('expected nsc and new evidence directory on A:')
    compiler, root = (absolute_a(value) for value in sys.argv[1:])
    repository = Path(__file__).resolve().parents[2]
    environment = setup(root, repository)
    failures = []
    for name, body in CASES.items():
        case = root / name
        case.mkdir()
        (case / 'Main.ns').write_text(PREFIX.replace('BODY', body), encoding='utf-8')
        project = case / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
        for level in ('O0', 'O2'):
            exe = case / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(exe), '--no-artifact-cache']
            print('CWD', repository, 'COMMAND', subprocess.list2cmdline(command), flush=True)
            result = subprocess.run(command, cwd=repository, env=environment, capture_output=True,
                                    text=True, errors='replace', timeout=120)
            if result.returncode:
                failures.append((name, level, 'compile', result.stdout + result.stderr))
                continue
            try:
                result = subprocess.run([str(exe)], cwd=repository, env=environment,
                                        capture_output=True, text=True, errors='replace', timeout=10)
                print('RUN', name, level, result.returncode, flush=True)
                if result.returncode != 42:
                    failures.append((name, level, 'run', result.returncode))
            except subprocess.TimeoutExpired:
                failures.append((name, level, 'timeout'))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
