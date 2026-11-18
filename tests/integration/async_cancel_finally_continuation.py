"""Directed regression for cancellation before and during async cleanup."""
import os
from pathlib import Path
import subprocess
import sys

PREFIX = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception {} }
class Program { public static int Log; public static int Calls;
 public static int Unknown() { Calls = Calls + 1; return 8; }
 BODY
}'''

GEN = '''public static AsyncIterator<int, Exception> Gen() {
 try { yield return 1; } finally { Log = Log * 10 + 1; yield return 5;
  Log = Log * 10 + 2; yield return 6; Log = Log * 10 + 3; } }'''
CASES = {
 'cancel_before_finally': (GEN + '''public static async int Main() { try {
 AsyncIterator<int, Exception> it = Gen(); bool first = await it.MoveNextAsync();
 if (!first || it.Current != 1) return 20; it.Cancel(); if (Log != 0) return 21;
 bool next = await it.MoveNextAsync(); if (next) return 22; it.Drop();
 return Log == 123 ? 42 : 23; } catch (Exception e) { return 29; } }''', True),
 'cancel_started_finally': (GEN + '''public static async int Main() { try {
 AsyncIterator<int, Exception> it = Gen(); bool first = await it.MoveNextAsync();
 bool second = await it.MoveNextAsync(); if (!first || !second || it.Current != 5 || Log != 1) return 20;
 it.Cancel(); if (Log != 1) return 21; bool next = await it.MoveNextAsync();
 if (next) return 22; it.Drop(); return Log == 123 ? 42 : 23;
 } catch (Exception e) { return 29; } }''', True),
 'normal': (GEN + '''public static async int Main() { try {
 AsyncIterator<int, Exception> it = Gen(); bool a = await it.MoveNextAsync(); if (!a || it.Current != 1) return 20;
 bool b = await it.MoveNextAsync(); if (!b || it.Current != 5 || Log != 1) return 21;
 bool c = await it.MoveNextAsync(); if (!c || it.Current != 6 || Log != 12) return 22;
 bool d = await it.MoveNextAsync(); if (d || Log != 123) return 23; it.Drop(); return 42;
 } catch (Exception e) { return 29; } }''', True),
 'unknown_cancel_differential': ('''public static AsyncIterator<int, Exception> Gen() {
 try { yield return 1; } finally { yield return Unknown(); Log = Log + 1; } }
 public static async int Main() { try { AsyncIterator<int, Exception> it = Gen();
 bool first = await it.MoveNextAsync(); if (!first) return 20; it.Cancel();
 bool next = await it.MoveNextAsync(); return 60 + (next ? 10 : 0) + Calls * 2 + Log;
 } catch (Exception e) { return 29; } }''', False),
}

def main():
    if os.name != 'nt' or len(sys.argv) != 3:
        print('usage: python async_cancel_finally_continuation.py <nsc-in-A> <new-output-dir-in-A>')
        return 2
    compiler = Path(sys.argv[1]).resolve()
    root = Path(sys.argv[2]).resolve()
    if compiler.anchor.lower() != 'a:\\' or root.anchor.lower() != 'a:\\' or not compiler.is_file(): return 2
    root.mkdir(parents=True, exist_ok=False)
    temporary = root / 'temp'
    temporary.mkdir()
    env = os.environ.copy()
    for name in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'): env[name] = str(temporary)
    repository = Path(__file__).resolve().parents[2]
    failures = []
    for name, (body, must_pass) in CASES.items():
        case = root / name
        case.mkdir()
        project = case / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        (case / 'Main.ns').write_text(PREFIX.replace('BODY', body), encoding='utf-8')
        for level in ('O0', 'O2'):
            exe = case / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(exe), '--no-artifact-cache']
            print('COMMAND', subprocess.list2cmdline(command), 'CWD', repository, flush=True)
            try:
                build = subprocess.run(command, cwd=repository, env=env, capture_output=True, text=True,
                                       errors='replace', timeout=120)
                print('COMPILE', name, level, build.returncode, build.stdout, build.stderr, flush=True)
                if build.returncode: failures.append((name, level, 'compile')); continue
                run = subprocess.run([str(exe)], cwd=repository, env=env, capture_output=True, text=True,
                                     errors='replace', timeout=10)
                print('RUN', name, level, run.returncode, run.stdout, run.stderr, flush=True)
                if must_pass and run.returncode != 42: failures.append((name, level, 'run', run.returncode))
            except subprocess.TimeoutExpired as error:
                print('TIMEOUT', name, level, repr(error.stdout), repr(error.stderr), flush=True)
                failures.append((name, level, 'timeout'))
    print('FAILURES', failures)
    return bool(failures)

if __name__ == '__main__':
    sys.exit(main())
