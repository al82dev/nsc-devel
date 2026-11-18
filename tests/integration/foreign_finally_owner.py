"""Check foreign finally replacement using NSC IR and an injected SEH observer.

The fixture replaces a method body in generated IR to raise a foreign exception;
it does not modify the generated cleanup being tested. Arguments are the NSC
executable, the Windows runtime library, and its import-library directory.
"""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


SOURCE = '''using NativeSharp.Core;
namespace NativeSharp.Core {
 public class Exception { public static int Drops; public int Id;
  public Exception(int id) { Id = id; } ~Exception() { Drops = Drops + 1; } }
}
GUARD
class Program {
 public static noinline int Fail() throws Exception { throw new Exception(2); }
 public static noinline int Work() throws Exception {
  OUTER
  try { throw new Exception(1); } finally { int ignored = Fail(); }
 }
 public static int Main() { int value = 0;
  try { value = Work(); } catch (Exception error) { value = error.Id; }
  return value == 2 && Exception.Drops == 2 ? 42 : 13;
 }
}
'''
OBSERVER = '''
@probe.selected = global i32 0
declare void @ExitProcess(i32) noreturn
define i32 @probe_foreign_filter(ptr %info, ptr %frame) {
  %record = load ptr, ptr %info
  %code = load i32, ptr %record
  store i32 %code, ptr @probe.selected
  %second = icmp eq i32 %code, -536870910
  %third = icmp eq i32 %code, -536870909
  %matches = or i1 %second, %third
  %result = zext i1 %matches to i32
  ret i32 %result
}
define i32 @probe_entry() personality ptr @__C_specific_handler {
entry:
  %ignored = invoke i32 @"__nsc_method_Probe$3A$3AProgram__Work"() to label %failed unwind label %dispatch
dispatch:
  %switch = catchswitch within none [label %pad] unwind to caller
pad:
  %caught = catchpad within %switch [ptr @probe_foreign_filter]
  catchret from %caught to label %verify
verify:
  %drops = load i32, ptr @"__nsc_static_Probe$3A$3ANativeSharp$2ECore$2EException__Drops"
  %drop.ok = icmp eq i32 %drops, 1
  %selected = load i32, ptr @probe.selected
  %code.ok = icmp eq i32 %selected, EXPECTED_CODE
  %current = call ptr @__nativesharp_exception_current()
  %tls.ok = icmp eq ptr %current, null
  %a = and i1 %drop.ok, %code.ok
  %basic.ok = and i1 %a, %tls.ok
  GUARD_CHECK
  %result = select i1 %correct, i32 42, i32 93
  call void @ExitProcess(i32 %result)
  unreachable
failed:
  call void @ExitProcess(i32 97)
  unreachable
}
'''
NATIVE_OBSERVER = '''
declare void @ExitProcess(i32) noreturn
define i32 @probe_filter(ptr %info, ptr %frame) {
  %native = call i32 @__nativesharp_eh_filter(ptr %info, ptr %frame)
  %is.native = icmp ne i32 %native, 0
  %record = load ptr, ptr %info
  %code = load i32, ptr %record
  %is.foreign = icmp eq i32 %code, -536870910
  %matches = or i1 %is.native, %is.foreign
  %result = zext i1 %matches to i32
  ret i32 %result
}
define i32 @probe_entry() personality ptr @__C_specific_handler {
entry:
  %ignored = invoke i32 @"__nsc_method_Probe$3A$3AProgram__Work"() to label %failed unwind label %dispatch
dispatch:
  %switch = catchswitch within none [label %pad] unwind to caller
pad:
  %caught = catchpad within %switch [ptr @probe_filter]
  catchret from %caught to label %verify
verify:
  %error = call ptr @__nativesharp_exception_current()
  %non.null = icmp ne ptr %error, null
  br i1 %non.null, label %inspect, label %failed
inspect:
  %id.addr = getelementptr i8, ptr %error, i64 8
  %id = load i32, ptr %id.addr
  %id.ok = icmp eq i32 %id, 3
  call void @__nativesharp_exception_clear()
  %descriptor = load ptr, ptr %error
  %entry.addr = getelementptr %nativesharp.class_descriptor, ptr %descriptor, i32 0, i32 5
  %drop = load ptr, ptr %entry.addr
  call void %drop(ptr %error)
  %drops = load i32, ptr @"__nsc_static_Probe$3A$3ANativeSharp$2ECore$2EException__Drops"
  %drops.ok = icmp eq i32 %drops, 2
  %current = call ptr @__nativesharp_exception_current()
  %tls.ok = icmp eq ptr %current, null
  %a = and i1 %id.ok, %drops.ok
  %basic.ok = and i1 %a, %tls.ok
  GUARD_CHECK
  %result = select i1 %correct, i32 42, i32 93
  call void @ExitProcess(i32 %result)
  unreachable
failed:
  call void @ExitProcess(i32 97)
  unreachable
}
'''


def run(command, cwd, expected):
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          result.stderr, flush=True)
    if result.returncode != expected:
        raise RuntimeError('unexpected exit ' + str(result.returncode))
    return result.stdout


def replace_body(text, name, body):
    pattern = r'(define \w+ @"' + re.escape(name) + r'"\([^)]*\)[^{]*\{).*?\n\}'
    result, count = re.subn(pattern, lambda match: match.group(1) + '\n' + body + '\n}',
                            text, count=1, flags=re.DOTALL)
    if count != 1:
        raise RuntimeError('missing unique function ' + name)
    return result


def run_native_destructor(compiler, runtime, imports, repository, root, outer, level, code, guard_check):
    """Compile the throwing destructor from source and inject only the foreign caller and observer."""
    code = code.replace('~Exception() { Drops = Drops + 1; }',
                        '~Exception() { Drops = Drops + 1; if (Id == 1) { int x = Gate.Read(); } }')
    code = code.replace('class Program {', 'class Gate { static Gate() throws Exception { throw new Exception(3); } '
                        'public static int Read() { return 1; } }\nclass Program {')
    (root / 'Main.ns').write_text(code, encoding='utf-8')
    ir = run([str(compiler), '--dump-project-oop-llvm', str(root / 'Probe.nsproj'), '-O' + level], repository, 0)
    ir = replace_body(ir, '__nsc_method_Probe$3A$3AProgram__Fail',
                      'entry:\n  call void @RaiseException(i32 -536870910, i32 0, i32 0, ptr null)\n  unreachable')
    ir += NATIVE_OBSERVER.replace('GUARD_CHECK', guard_check)
    tag = 'native-destructor' + ('-outer' if outer else '') + '-O' + level
    fixture, obj, exe = (root / (tag + suffix) for suffix in ('.ll', '.obj', '.exe'))
    fixture.write_text(ir, encoding='utf-8')
    run(['clang', '-O' + level, '-c', '-x', 'ir', str(fixture), '-o', str(obj)], repository, 0)
    libraries = [str(imports / (name + '.import.lib')) for name in
                 ('KERNEL32', 'KERNELBASE', 'SHELL32', 'UCRTBASE')]
    run(['lld-link', '/subsystem:console', '/entry:probe_entry', str(obj), str(runtime),
         *libraries, '/out:' + str(exe)], repository, 0)
    run([str(exe)], repository, 42)


def main():
    if os.name != 'nt' or len(sys.argv) != 4:
        raise SystemExit('usage: foreign_finally_owner.py NSC RUNTIME_LIB IMPORT_LIBRARY_DIRECTORY')
    compiler, runtime, imports = map(Path, sys.argv[1:])
    repository = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix='nsc-foreign-owner-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        for outer in (False, True):
            guard = ('class Guard { public static int Drops; ~Guard() { '
                     'if (Exception.Drops == 1) Drops = Drops + 1; else Drops = 99; } }') if outer else ''
            code = SOURCE.replace('GUARD', guard).replace('OUTER', 'Guard guard = new Guard();' if outer else '')
            (root / 'Main.ns').write_text(code, encoding='utf-8')
            for level in ('0', '2'):
                (root / 'Main.ns').write_text(code, encoding='utf-8')
                ir = run([str(compiler), '--dump-project-oop-llvm', str(project), '-O' + level], repository, 0)
                ir = replace_body(ir, '__nsc_method_Probe$3A$3AProgram__Fail',
                                  'entry:\n  call void @RaiseException(i32 -536870910, i32 0, i32 0, ptr null)\n'
                                  '  unreachable')
                guard_check = ('%guard.drops = load i32, ptr @"__nsc_static_Probe$3A$3AGuard__Drops"\n'
                               '  %guard.ok = icmp eq i32 %guard.drops, 1\n'
                               '  %correct = and i1 %basic.ok, %guard.ok') if outer else '%correct = and i1 %basic.ok, true'
                ir += OBSERVER.replace('GUARD_CHECK', guard_check)
                name = '__nsc_dtor_body_Probe$3A$3ANativeSharp$2ECore$2EException___Exception'
                pattern = r'(define void @"' + re.escape(name) + r'"\(ptr %this\)[^{]*\{)(.*?)(\n})'
                match = re.search(pattern, ir, re.DOTALL)
                if match is None or match.group(2).count('  ret void') != 1:
                    raise RuntimeError('missing unique destructor')
                body = match.group(2).replace('  ret void',
                    '  call void @RaiseException(i32 -536870909, i32 0, i32 0, ptr null)\n  unreachable')
                throwing = ir[:match.start(2)] + body + ir[match.end(2):]
                for name, text, error_code in (('foreign', ir, '-536870910'),
                                                ('destructor-foreign', throwing, '-536870909')):
                    tag = name + ('-outer' if outer else '') + '-O' + level
                    fixture, obj, exe = (root / (tag + suffix) for suffix in ('.ll', '.obj', '.exe'))
                    fixture.write_text(text.replace('EXPECTED_CODE', error_code), encoding='utf-8')
                    run(['clang', '-O' + level, '-c', '-x', 'ir', str(fixture), '-o', str(obj)], repository, 0)
                    libraries = [str(imports / (name + '.import.lib')) for name in
                                 ('KERNEL32', 'KERNELBASE', 'SHELL32', 'UCRTBASE')]
                    run(['lld-link', '/subsystem:console', '/entry:probe_entry', str(obj), str(runtime),
                         *libraries, '/out:' + str(exe)], repository, 0)
                    run([str(exe)], repository, 42)
                run_native_destructor(compiler, runtime, imports, repository, root, outer, level, code, guard_check)
    print('foreign_finally_owner: PASS', flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
