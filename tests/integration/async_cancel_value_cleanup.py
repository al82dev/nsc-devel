"""Verify destruction of a constructed value discarded during cancellation."""

from pathlib import Path
import subprocess
import sys

from isolated_a_runner import absolute_a, setup


PREFIX = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public static int Drops; ~Exception() { Drops = Drops + 1; } } }
struct Payload {
 public static int Drops; public static int Builds; public int Id;
 public Payload() THROWS { Builds = Builds + 1; Id = 7; FAIL }
 ~Payload() { Drops = Drops + 1; }
}
class Program {
 public static int Log;
 public static AsyncIterator<Payload, Exception> Gen() throws Exception {
  try { yield return default(Payload); }
  finally { Log = 1; yield return new Payload(); Log = 12; }
 }
 BODY
}'''

SUCCESS = '''public static async int Main() { try {
 AsyncIterator<Payload, Exception> it = Gen();
 if (!await it.MoveNextAsync()) return 20;
 it.Cancel(); if (await it.MoveNextAsync()) return 22;
 it.Drop(); return Log == 12 && Payload.Builds == 1 && Payload.Drops == 1 ? 42 : 23;
 } catch (Exception e) { return 29; } }'''

THROW = '''public static async int Main() { int caught = 0; {
 AsyncIterator<Payload, Exception> it = Gen();
 try { if (!await it.MoveNextAsync()) return 20; it.Cancel();
  bool next = await it.MoveNextAsync(); return 22;
 } catch (Exception e) { caught = 1; }
 }
 return caught == 1 && Log == 1 && Payload.Builds == 1 && Payload.Drops == 0 &&
  Exception.Drops == 1 ? 42 : 23; }'''

NORMAL = '''public static async int Main() { try {
 { AsyncIterator<Payload, Exception> it = Gen();
 if (!await it.MoveNextAsync()) return 20;
 if (!await it.MoveNextAsync()) return 21;
 { Payload received = move(it.Current); }
 if (await it.MoveNextAsync()) return 22;
 it.Drop(); }
 return Log == 12 && Payload.Builds == 1 && Payload.Drops == 1 ? 42 : 23;
 } catch (Exception e) { return 29; } }'''

STARTED = '''public static async int Main() { try {
 { AsyncIterator<Payload, Exception> it = Gen();
 if (!await it.MoveNextAsync()) return 20;
 if (!await it.MoveNextAsync()) return 21;
 { Payload received = move(it.Current); }
 it.Cancel(); if (await it.MoveNextAsync()) return 22;
 it.Drop(); }
 return Log == 12 && Payload.Builds == 1 && Payload.Drops == 1 ? 42 : 23;
 } catch (Exception e) { return 29; } }'''


def main():
    if len(sys.argv) != 3:
        raise SystemExit('expected nsc and new evidence directory on A:')
    compiler, root = (absolute_a(value) for value in sys.argv[1:])
    repository = Path(__file__).resolve().parents[2]
    environment = setup(root, repository)
    failures = []
    cases = (('success', '', '', SUCCESS),
             ('throw', 'throws Exception', 'throw new Exception();', THROW),
             ('owned_success', '', '', SUCCESS.replace('Payload.Drops == 1 ?',
                                                        'Payload.Drops == 1 && Thing.Drops == 1 ?')),
             ('owned_throw', 'throws Exception', 'throw new Exception();',
              THROW.replace('Exception.Drops == 1 ?', 'Exception.Drops == 1 && Thing.Drops == 1 ?')),
             ('move_success', '', '', SUCCESS),
             ('move_throw', 'throws Exception', 'throw new Exception();', THROW),
             ('move_normal', '', '', NORMAL),
             ('move_started', '', '', STARTED),
             ('move_owned_success', '', '', SUCCESS.replace('Payload.Drops == 1 ?',
                                                             'Payload.Drops == 1 && Thing.Drops == 1 ?')),
             ('move_owned_started', '', '', STARTED.replace('Payload.Drops == 1 ?',
                                                            'Payload.Drops == 1 && Thing.Drops == 1 ?')),
             ('move_owned_throw', 'throws Exception', 'throw new Exception();',
              THROW.replace('Exception.Drops == 1 ?', 'Exception.Drops == 1 && Thing.Drops == 1 ?')))
    for name, throws, fail, body in cases:
        case = root / name
        case.mkdir()
        source = PREFIX.replace('THROWS', throws).replace('FAIL', fail).replace('BODY', body)
        if 'owned_' in name:
            source = source.replace('struct Payload {',
                'class Thing { public static int Drops; ~Thing() { Drops = Drops + 1; } }\n'
                'struct Payload { public owned Thing Field;')
            source = source.replace('Builds = Builds + 1; Id = 7;',
                                    'Builds = Builds + 1; Id = 7; Field = new Thing();')
        if name.startswith('move_'):
            source = source.replace('yield return new Payload();',
                                    'Payload value = new Payload(); yield return move(value);')
        (case / 'Main.ns').write_text(source, encoding='utf-8')
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
                failures.append((name, level, 'compile', result.stdout + result.stderr))
                continue
            result = subprocess.run([str(executable)], cwd=repository, env=environment,
                                    capture_output=True, text=True, errors='replace', timeout=10)
            print('RUN', name, level, result.returncode, flush=True)
            if result.returncode != 42:
                failures.append((name, level, 'run', result.returncode))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
