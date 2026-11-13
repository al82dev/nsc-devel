"""Directed regression for mixed owned/alias results during cancellation.

The original reproduction is retained under A:/nsc-fixes-evidencia. This
runner takes an isolated compiler and an unused output directory on A:.
"""

import os
from pathlib import Path
import subprocess
import sys


PREFIX = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public static int Drops; ~Exception() { Drops = Drops + 1; } } }
class Payload { public static int Builds; public static int Drops; public static int DropTrace; public int Id;
 public Payload() { Builds = Builds + 1; Id = Builds; }
 ~Payload() { Drops = Drops + 1; DropTrace = DropTrace * 10 + Id; } }
class Program { public static Payload Shared; public static int Effects; public static int Calls; public static int Choices;
 public static bool Choose(bool value) { Choices = Choices + 1; return value; }
 public static owned Payload Make() throws Exception { Calls = Calls + 1; FAIL return new Payload(); }
 public static AsyncIterator<Payload, Exception> Gen(bool choose) throws Exception {
  try { yield return Shared; } finally { Effects = Effects + 1;
   yield return Choose(choose) ? Make() : Shared; Effects = Effects + 10; }
 }
 BODY
}'''

MAIN = '''public static async int Main() { try {
 Shared = new Payload(); { AsyncIterator<Payload, Exception> it = Gen(CHOOSE);
 bool first = await it.MoveNextAsync(); if (!first || it.Current.Id != 1) return 20;
 it.Cancel(); if (Effects != 0 || Payload.Drops != 0 || Calls != 0) return 21;
 bool next = await it.MoveNextAsync(); if (next) return 22; it.Drop(); }
 return Effects == 11 && Choices == 1 && Calls == CALLS && Payload.Builds == BUILDS &&
  Payload.Drops == DROPS && Payload.DropTrace == TRACE ? 42 : 23;
 } catch (Exception error) { return 29; } }'''

THROWING = '''public static async int Main() { int caught = 0;
 Shared = new Payload(); { AsyncIterator<Payload, Exception> it = Gen(true);
 try { bool first = await it.MoveNextAsync(); if (!first) return 20; it.Cancel();
 bool next = await it.MoveNextAsync(); return 21; }
 catch (Exception error) { caught = 1; } }
 return caught == 1 && Effects == 1 && Choices == 1 && Calls == 1 && Payload.Builds == 1 &&
  Payload.Drops == 0 && Payload.DropTrace == 0 && Exception.Drops == 1 ? 42 : 23; }'''

STARTED_ALIAS = MAIN.replace(
    ' it.Cancel(); if (Effects != 0',
    ' if (!await it.MoveNextAsync() || Effects != 1 || it.Current.Id != 1) return 24;\n'
    ' it.Cancel(); if (Effects != 1').replace('Choices == 1', 'Choices == 0')

NORMAL_LOCAL_ALIAS = '''public static async int Main() { try {
 Shared = new Payload(); { AsyncIterator<Payload, Exception> it = Gen(true);
 if (!await it.MoveNextAsync() || it.Current.Id != 1) return 20;
 if (!await it.MoveNextAsync() || it.Current.Id != 2) return 21;
 if (await it.MoveNextAsync()) return 22; it.Drop(); }
 return Effects == 11 && Choices == 0 && Calls == 0 && Payload.Builds == 2 &&
  Payload.Drops == 1 && Payload.DropTrace == 2 ? 42 : 23;
 } catch (Exception error) { return 29; } }'''

NORMAL_LOCAL_BORROW = NORMAL_LOCAL_ALIAS.replace('it.Current.Id != 2', 'it.Current.Id != 1') \
    .replace('Payload.Builds == 2', 'Payload.Builds == 1') \
    .replace('Payload.Drops == 1 && Payload.DropTrace == 2', 'Payload.Drops == 0 && Payload.DropTrace == 0')

NEW_THROW = (THROWING.replace('Shared = new Payload();',
             'try { Shared = new Payload(); } catch (Exception init) { return 30; }')
             .replace('Choices == 1', 'Choices == 0').replace('Calls == 1', 'Calls == 0')
             .replace('Payload.Builds == 1', 'Payload.Builds == 2'))

CASES = (
    ('owned', 'true', '', '1', '2', '1', '2', MAIN),
    ('alias', 'false', '', '0', '1', '0', '0', MAIN),
    ('throw', 'true', 'throw new Exception();', '1', '1', '0', '0', THROWING),
    ('direct_owned', 'true', '', '1', '2', '1', '2', MAIN.replace('Choices == 1', 'Choices == 0')),
    ('direct_new', 'true', '', '0', '2', '1', '2', MAIN.replace('Choices == 1', 'Choices == 0')),
    ('direct_move', 'true', '', '0', '2', '1', '2', MAIN.replace('Choices == 1', 'Choices == 0')),
    ('direct_local_alias', 'true', '', '0', '2', '1', '2', MAIN.replace('Choices == 1', 'Choices == 0')),
    ('direct_local_alias_started', 'true', '', '0', '2', '1', '2',
     STARTED_ALIAS.replace('it.Current.Id != 1) return 24', 'it.Current.Id != 2) return 24')),
    ('direct_local_alias_normal', 'true', '', '0', '2', '1', '2', NORMAL_LOCAL_ALIAS),
    ('direct_local_borrow', 'false', '', '0', '1', '0', '0', MAIN.replace('Choices == 1', 'Choices == 0')),
    ('direct_local_borrow_started', 'false', '', '0', '1', '0', '0', STARTED_ALIAS),
    ('direct_local_borrow_normal', 'false', '', '0', '1', '0', '0', NORMAL_LOCAL_BORROW),
    ('direct_alias', 'false', '', '0', '1', '0', '0', MAIN.replace('Choices == 1', 'Choices == 0')),
    ('direct_throw', 'true', 'throw new Exception();', '1', '1', '0', '0',
     THROWING.replace('Choices == 1', 'Choices == 0')),
    ('direct_alias_started', 'false', '', '0', '1', '0', '0', STARTED_ALIAS),
    ('direct_new_throw', 'true', '', '0', '2', '0', '0', NEW_THROW),
)

EXPRESSIONS = {'direct_owned': 'Make()', 'direct_new': 'new Payload()', 'direct_move': 'move(value)',
               'direct_local_alias': 'value', 'direct_local_alias_started': 'value',
               'direct_local_alias_normal': 'value',
               'direct_local_borrow': 'value', 'direct_local_borrow_started': 'value',
               'direct_local_borrow_normal': 'value',
               'direct_alias': 'Shared', 'direct_throw': 'Make()',
               'direct_alias_started': 'Shared', 'direct_new_throw': 'new Payload()'}


def main():
    if os.name != 'nt' or len(sys.argv) != 3:
        print('usage: python async_yield_mixed_cancellation.py <compiler> <new-output-dir>')
        return 2
    compiler = Path(sys.argv[1]).resolve()
    root = Path(sys.argv[2]).resolve()
    repository = Path(__file__).resolve().parents[2]
    if compiler.anchor.lower() != 'a:\\' or root.anchor.lower() != 'a:\\' or not compiler.is_file():
        return 2
    root.mkdir(parents=True, exist_ok=False)
    temporary = root / 'temp'
    temporary.mkdir()
    env = os.environ.copy()
    for name in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'):
        env[name] = str(temporary)
    env['PYTHONDONTWRITEBYTECODE'] = '1'
    failures = []
    for name, choose, fail, calls, builds, drops, trace, source in CASES:
        case_root = root / name
        case_root.mkdir()
        project = case_root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        body = (source.replace('CHOOSE', choose).replace('CALLS', calls).replace('BUILDS', builds)
                .replace('DROPS', drops).replace('TRACE', trace))
        prefix = PREFIX.replace('Choose(choose) ? Make() : Shared',
                                EXPRESSIONS.get(name, 'Choose(choose) ? Make() : Shared'))
        if name == 'direct_move':
            prefix = prefix.replace('yield return move(value);',
                                    'owned Payload value = new Payload(); yield return move(value);')
        if name.startswith('direct_local_alias'):
            prefix = prefix.replace('yield return value;',
                                    'owned Payload value = new Payload(); yield return value;')
        if name.startswith('direct_local_borrow'):
            prefix = prefix.replace('yield return value;', 'Payload value = Shared; yield return value;')
        if name == 'direct_new_throw':
            prefix = prefix.replace('public Payload() { Builds = Builds + 1; Id = Builds; }',
                                    'public Payload() throws Exception { Builds = Builds + 1; Id = Builds; '
                                    'if (Builds == 2) throw new Exception(); }')
        (case_root / 'Main.ns').write_text(prefix.replace('FAIL', fail).replace('BODY', body), encoding='utf-8')
        for level in ('O0', 'O2'):
            exe = case_root / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(exe), '--no-artifact-cache']
            print('COMMAND', subprocess.list2cmdline(command), 'CWD', repository, flush=True)
            try:
                build = subprocess.run(command, cwd=repository, env=env, capture_output=True, text=True,
                                       errors='replace', timeout=120)
            except subprocess.TimeoutExpired as error:
                print('COMPILE TIMEOUT', name, level, 'stdout=', error.stdout, 'stderr=', error.stderr, flush=True)
                failures.append((name, level, 'compile-timeout'))
                continue
            print('COMPILE', name, level, build.returncode, build.stdout, build.stderr, flush=True)
            if build.returncode:
                failures.append((name, level, 'compile'))
                continue
            try:
                run = subprocess.run([str(exe)], cwd=repository, env=env, capture_output=True, text=True,
                                     errors='replace', timeout=10)
            except subprocess.TimeoutExpired as error:
                print('RUN TIMEOUT', name, level, 'stdout=', error.stdout, 'stderr=', error.stderr, flush=True)
                failures.append((name, level, 'run-timeout'))
                continue
            print('RUN', name, level, run.returncode, run.stdout, run.stderr, flush=True)
            if run.returncode != 42:
                failures.append((name, level, 'run', run.returncode))
    print('FAILURES', failures)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
