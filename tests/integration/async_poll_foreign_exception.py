"""Observe a foreign exception through NSC-generated async poll IR.

Only the body of Fail and the outer SEH observer are injected. The poll and
its cleanup remain exactly as emitted by NSC. This is a hybrid ABI test.
"""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
from isolated_a_runner import absolute_a, setup

from foreign_finally_owner import replace_body

SOURCE = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public int Id;
public Exception(int id) { Id = id; } } }
class Guard { public static int Drops; ~Guard() { Drops = Drops + 1; } }
class Program { public static int Log;
public static noinline int Fail() throws Exception { throw new Exception(7); }
public static async int Work() throws Exception { yield; BODY }
public static async int Main() { if (Guard.Drops + Log != 0) return 3;
try { int ignored = await Work(); return 1; }
catch (Exception error) { return 2; } } }
'''

OBSERVER = '''
declare void @ExitProcess(i32) noreturn
@probe.foreign.code = global i32 0
define i32 @probe_foreign_filter(ptr %info, ptr %frame) {
  %record = load ptr, ptr %info
  %code = load i32, ptr %record
  store i32 %code, ptr @probe.foreign.code
  %match = icmp eq i32 %code, -536870910
  %result = zext i1 %match to i32
  ret i32 %result
}
define i32 @probe_entry() personality ptr @__C_specific_handler {
entry:
  %ignored = invoke i32 @"__nsc_method_Probe$3A$3AProgram__Main$entry"()
      to label %failed unwind label %dispatch
dispatch:
  %switch = catchswitch within none [label %pad] unwind to caller
pad:
  %caught = catchpad within %switch [ptr @probe_foreign_filter]
  catchret from %caught to label %verify
verify:
  %code = load i32, ptr @probe.foreign.code
  %code.ok = icmp eq i32 %code, -536870910
  %drops = load i32, ptr @"__nsc_static_Probe$3A$3AGuard__Drops"
  %drops.ok = icmp eq i32 %drops, EXPECTED_DROPS
  %trace = load i32, ptr @"__nsc_static_Probe$3A$3AProgram__Log"
  %trace.ok = icmp eq i32 %trace, EXPECTED_TRACE
  %current = call ptr @__nativesharp_exception_current()
  %tls.ok = icmp eq ptr %current, null
  %a = and i1 %code.ok, %drops.ok
  %b = and i1 %trace.ok, %tls.ok
  %ok = and i1 %a, %b
  %result = select i1 %ok, i32 42, i32 93
  call void @ExitProcess(i32 %result)
  unreachable
failed:
  call void @ExitProcess(i32 97)
  unreachable
}
'''


def run(command, cwd, expected):
    print('COMMAND', subprocess.list2cmdline(command), 'CWD', cwd, flush=True)
    result = subprocess.run(command, cwd=cwd, env=ENVIRONMENT, capture_output=True, text=True, errors='replace', timeout=120)
    print('EXIT', result.returncode, result.stderr, flush=True)
    if result.returncode != expected:
        raise RuntimeError('unexpected exit ' + str(result.returncode))
    return result.stdout


def main():
    if os.name != 'nt' or len(sys.argv) != 5:
        return 77
    compiler, runtime, imports, evidence = (absolute_a(value) for value in sys.argv[1:])
    repository = Path(__file__).resolve().parents[2]
    global ENVIRONMENT
    ENVIRONMENT = setup(evidence, repository)
    libraries = [str(imports / (name + '.import.lib')) for name in ('KERNEL32', 'KERNELBASE', 'SHELL32', 'UCRTBASE')]
    with tempfile.TemporaryDirectory(prefix='nsc-async-foreign-', dir=evidence) as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        for cleanup in (False, True):
            body = ('owned Guard guard = new Guard(); try { return Fail(); } '
                    'finally { Log = Log + 1; }') if cleanup else 'return Fail();'
            (root / 'Main.ns').write_text(SOURCE.replace('BODY', body), encoding='utf-8')
            ir = run([str(compiler), '--dump-project-oop-llvm', str(project)], repository, 0)
            ir = replace_body(ir, '__nsc_method_Probe$3A$3AProgram__Fail',
                              'entry:\n  call void @RaiseException(i32 -536870910, i32 0, i32 0, ptr null)\n'
                              '  unreachable')
            ir += OBSERVER.replace('EXPECTED_DROPS', '1' if cleanup else '0').replace(
                'EXPECTED_TRACE', '1' if cleanup else '0')
            fixture = root / ('foreign-cleanup.ll' if cleanup else 'foreign-plain.ll')
            fixture.write_text(ir, encoding='utf-8')
            for level in ('0', '2'):
                obj = root / ('foreign-' + str(cleanup) + '-O' + level + '.obj')
                exe = root / ('foreign-' + str(cleanup) + '-O' + level + '.exe')
                run(['clang', '-O' + level, '-c', '-x', 'ir', str(fixture), '-o', str(obj)], repository, 0)
                run(['lld-link', '/subsystem:console', '/entry:probe_entry', str(obj), str(runtime),
                     *libraries, '/out:' + str(exe)], repository, 0)
                run([str(exe)], repository, 42)
    return 0


if __name__ == '__main__':
    sys.exit(main())
