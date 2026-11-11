"""Exercise the unchanged heap-overflow example through the ASan driver."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def invoke(command, cwd, env=None):
    result = subprocess.run([str(item) for item in command], cwd=cwd,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace', timeout=120, env=env)
    print('COMMAND', subprocess.list2cmdline([str(item) for item in command]),
          'EXIT', result.returncode, result.stdout, flush=True)
    return result


def main():
    clang = os.environ.get('NSC_TEST_ASAN_CLANG')
    runtime = os.environ.get('NSC_TEST_ASAN_RUNTIME')
    if os.name != 'nt' or not clang or not runtime:
        print('SKIP: set NSC_TEST_ASAN_CLANG and NSC_TEST_ASAN_RUNTIME on Windows')
        return 77
    compiler, example = map(Path, sys.argv[1:3])
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-asan-heap-example-') as temporary:
        root = Path(temporary)
        for name in ('Program.ns', 'OutOfBounds.nsproj'):
            shutil.copyfile(example / name, root / name)
        for level in ('O0', 'O2'):
            executable = root / ('out-of-bounds-' + level + '.exe')
            result = invoke([compiler, '--emit-project-oop-exe', root / 'OutOfBounds.nsproj',
                             '-' + level, '-o', executable, '--sanitize-address',
                             '--asan-clang=' + clang, '--asan-runtime=' + runtime], root)
            if result.returncode:
                failures.append(level + ': build')
                continue
            result = invoke([executable], root)
            detected = ('heap-buffer-overflow' in result.stdout or
                        'use-after-poison' in result.stdout)
            if result.returncode == 0 or 'ERROR: AddressSanitizer' not in result.stdout or not detected:
                failures.append(level + ': ASan did not report the invalid heap access')
        for case, body, expected in (
                ('valid', 'pointer[3] = 42; int result = pointer[3]; delete[] values; return result;', None),
                ('use-after-free', 'delete[] values; pointer[0] = 42; return 0;', 'heap-use-after-free')):
            case_root = root / case
            case_root.mkdir()
            shutil.copyfile(example / 'OutOfBounds.nsproj', case_root / 'OutOfBounds.nsproj')
            (case_root / 'Program.ns').write_text(
                'int Main() { int[] values = new int[4]; int* pointer = &values[0]; ' + body + ' }\n',
                encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = case_root / (case + '-' + level + '.exe')
                result = invoke([compiler, '--emit-project-oop-exe', case_root / 'OutOfBounds.nsproj',
                                 '-' + level, '-o', executable, '--sanitize-address',
                                 '--asan-clang=' + clang, '--asan-runtime=' + runtime], case_root)
                if result.returncode:
                    failures.append(case + ' ' + level + ': build')
                    continue
                result = invoke([executable], case_root)
                if expected is None:
                    if result.returncode != 42 or 'ERROR: AddressSanitizer' in result.stdout:
                        failures.append(case + ' ' + level + ': valid allocation failed')
                    plain = case_root / ('plain-' + level + '.exe')
                    result = invoke([compiler, '--emit-project-oop-exe', case_root / 'OutOfBounds.nsproj',
                                     '-' + level, '-o', plain], case_root)
                    if result.returncode or invoke([plain], case_root).returncode != 42:
                        failures.append(case + ' ' + level + ': ordinary allocation failed')
                elif result.returncode == 0 or expected not in result.stdout:
                    failures.append(case + ' ' + level + ': ASan did not report ' + expected)
        configured_runtime = os.environ.get('NSC_WINDOWS_RUNTIME_LIBRARY')
        if configured_runtime:
            incompatible = root / 'incompatible-runtime.lib'
            result = invoke(['llvm-objcopy', '--redefine-sym=HeapReAlloc=unsupported_HeapReAlloc',
                             configured_runtime, incompatible], root)
            if result.returncode:
                failures.append('cannot prepare incompatible runtime fixture')
            else:
                env = os.environ.copy()
                env['NSC_WINDOWS_RUNTIME_LIBRARY'] = str(incompatible)
                executable = root / 'incompatible-runtime.exe'
                result = invoke([compiler, '--emit-project-oop-exe', root / 'OutOfBounds.nsproj',
                                 '-O0', '-o', executable, '--sanitize-address',
                                 '--asan-clang=' + clang, '--asan-runtime=' + runtime], root, env)
                if (result.returncode == 0 or executable.exists() or
                        'incompatible heap references' not in result.stdout):
                    failures.append('incompatible runtime was not rejected')
            alternate = root / 'alternate-runtime.lib'
            object_file = root / 'alternate-runtime.obj'
            commands = (
                ['llc', '-filetype=obj', example / 'custom_runtime.ll', '-o', object_file],
                ['llvm-objcopy', '--redefine-sym=__nativesharp_alloc=__nsc_original_alloc',
                 configured_runtime, alternate],
                ['llvm-ar', 'r', alternate, object_file],
            )
            if any(invoke(command, root).returncode for command in commands):
                failures.append('cannot prepare alternate allocator runtime')
            else:
                env = os.environ.copy()
                env['NSC_WINDOWS_RUNTIME_LIBRARY'] = str(alternate)
                executable = root / 'alternate-runtime.exe'
                result = invoke([compiler, '--emit-project-oop-exe', root / 'OutOfBounds.nsproj',
                                 '-O0', '-o', executable, '--sanitize-address',
                                 '--asan-clang=' + clang, '--asan-runtime=' + runtime], root, env)
                if (result.returncode == 0 or executable.exists() or
                        'unverified allocation contract' not in result.stdout):
                    failures.append('alternate allocator runtime was not rejected')
        else:
            print('NOTE: incompatible runtime check requires NSC_WINDOWS_RUNTIME_LIBRARY')
        failing_tool_root = root / 'failing tool'
        failing_tool_root.mkdir()
        for name in ('Program.ns', 'OutOfBounds.nsproj'):
            shutil.copyfile(example / name, failing_tool_root / name)
        fake_tool = failing_tool_root / 'llvm-objcopy.exe'
        result = invoke([clang, '--target=x86_64-pc-windows-msvc', '-fuse-ld=lld',
                         example / 'failing_objcopy.ll', '-o', fake_tool], failing_tool_root)
        if result.returncode:
            failures.append('cannot prepare failing objcopy fixture')
        else:
            executable = failing_tool_root / 'probe.exe'
            result = invoke([compiler, '--emit-project-oop-exe', failing_tool_root / 'OutOfBounds.nsproj',
                             '-O0', '-o', executable, '--sanitize-address',
                             '--asan-clang=' + clang, '--asan-runtime=' + runtime], failing_tool_root)
            if result.returncode or not executable.exists():
                failures.append('configured objcopy was shadowed by a project-local executable')
            else:
                result = invoke([executable], failing_tool_root)
                if result.returncode == 0 or 'heap-buffer-overflow' not in result.stdout:
                    failures.append('configured objcopy did not produce a working ASan executable')
    if failures:
        print('\n'.join(failures), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
