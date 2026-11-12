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

CASES = (
    ('owned', 'true', '', '1', '2', '1', '2', MAIN),
    ('alias', 'false', '', '0', '1', '0', '0', MAIN),
    ('throw', 'true', 'throw new Exception();', '1', '1', '0', '0', THROWING),
)


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
        (case_root / 'Main.ns').write_text(PREFIX.replace('FAIL', fail).replace('BODY', body), encoding='utf-8')
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
