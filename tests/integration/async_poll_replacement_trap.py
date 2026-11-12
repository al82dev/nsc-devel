"""Directed control for a third exception while replacing an async poll error.

Windows runtime buildTrap maps its reason to ExitProcess(reason + 128).
The existing cleanup double-exception guard uses reason 20. This assertion
therefore depends on linking that runtime, rather than inventing a crash code.
"""
import os
import hashlib
import json
from contextlib import nullcontext
from pathlib import Path
import subprocess
import sys
from isolated_a_runner import absolute_a, setup

SOURCE = r'''using NativeSharp.Core;
namespace NativeSharp.Core {
 public class Exception {
  public static int Builds; public static int Drops; public static int Trace;
  public int Id; public Exception(int id) { Id = id; Builds = Builds + 1; }
  ~Exception() { Drops = Drops + 1; Trace = Trace * 10 + Id; REPLACEMENT }
 }
}
class Gate9 { static Gate9() throws Exception { throw new Exception(9); }
 public static int Read() { return 1; } }
class Guard { public static int Drops; ~Guard() { Drops = Drops + 1; } }
class Program {
 public static int Fail() throws Exception { throw new Exception(7); }
 public static int Replace() throws Exception { throw new Exception(8); }
 public static async int Work() throws Exception { yield; owned Guard guard = new Guard();
  try { return Fail(); } finally { int ignored = Replace(); } }
 public static async int Main() { int caught = 0;
  try { int ignored = await Work(); return 1; } catch (Exception e) { caught = e.Id; }
  return caught == 8 && Exception.Builds == 2 && Exception.Drops == 2 &&
    Exception.Trace == 78 && Guard.Drops == 1 ? 42 : 2;
 }
}
'''
PROJECT = ('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>')


def main():
    if os.name != 'nt' or len(sys.argv) != 5:
        print('usage: async_poll_replacement_trap.py <nsc-A> <new-evidence-dir-A> <runtime-lib-A> <imports-dir-A>')
        return 77
    compiler = absolute_a(sys.argv[1])
    evidence = absolute_a(sys.argv[2])
    runtime = absolute_a(sys.argv[3])
    imports = absolute_a(sys.argv[4])
    if not compiler.is_file() or not runtime.is_file() or not imports.is_dir():
        raise ValueError('compiler, runtime and imports must exist')
    required = ('KERNEL32.import.lib', 'KERNELBASE.import.lib', 'SHELL32.import.lib', 'UCRTBASE.import.lib')
    libraries = [absolute_a(imports / name) for name in required]
    if any(not path.is_file() for path in libraries):
        raise ValueError('one or more runtime import libraries are missing')
    repository = Path(__file__).resolve().parents[2]
    environment = setup(evidence, repository)
    environment['NSC_WINDOWS_RUNTIME_LIBRARY'] = str(runtime)
    environment['NSC_WINDOWS_RUNTIME_IMPORT_LIBRARY_DIRECTORY'] = str(imports)
    inputs = [compiler, runtime] + libraries
    def hashes():
        return {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}
    before = hashes()
    (evidence / 'inputs-before.json').write_text(json.dumps(before, indent=2), encoding='utf-8')
    print('INPUT_SHA256_BEFORE', before, flush=True)
    # src/Runtime/WindowsRuntimeBuilder.cpp buildTrap and cleanup guard reason.
    trap_exit = 128 + 20
    failures = []
    with nullcontext(evidence / 'cases') as root:
        root.mkdir()
        for name, statement, expected in (
                ('replacement_control', '', 42),
                ('third_exception', 'if (Id == 7) { int ignored = Gate9.Read(); }', trap_exit)):
            case = root / name
            case.mkdir()
            (case / 'Main.ns').write_text(SOURCE.replace('REPLACEMENT', statement), encoding='utf-8')
            project = case / 'Probe.nsproj'
            project.write_text(PROJECT, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = case / (level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                print('COMMAND', subprocess.list2cmdline(command), 'CWD', repository, flush=True)
                build = subprocess.run(command, cwd=repository, env=environment, capture_output=True, text=True,
                                       errors='replace', timeout=120)
                (case / (level + '-build.log')).write_text(
                    'COMMAND ' + subprocess.list2cmdline(command) + '\nCWD ' + str(repository) +
                    '\nEXIT ' + str(build.returncode) + '\n' + build.stdout + build.stderr, encoding='utf-8')
                if build.returncode:
                    failures.append((name, level, 'compile', build.returncode, build.stdout + build.stderr))
                    continue
                result = subprocess.run([str(executable)], cwd=repository, env=environment, capture_output=True, text=True,
                                        errors='replace', timeout=10)
                (case / (level + '-run.log')).write_text(
                    'COMMAND ' + subprocess.list2cmdline([str(executable)]) + '\nCWD ' + str(repository) +
                    '\nEXIT ' + str(result.returncode) + '\n' + result.stdout + result.stderr, encoding='utf-8')
                print('RUN', name, level, result.returncode, 'EXPECTED', expected, flush=True)
                if result.returncode != expected:
                    failures.append((name, level, 'run', result.returncode, result.stdout + result.stderr))
    after = hashes()
    (evidence / 'inputs-after.json').write_text(json.dumps(after, indent=2), encoding='utf-8')
    print('INPUT_SHA256_AFTER', after, flush=True)
    if before != after:
        failures.append(('inputs', 'changed'))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
