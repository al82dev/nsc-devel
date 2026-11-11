"""Exercise ASan with the executable builder's generated runtime."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(command, cwd):
    result = subprocess.run([str(item) for item in command], cwd=cwd,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace', timeout=180)
    print('COMMAND', subprocess.list2cmdline([str(item) for item in command]),
          'EXIT', result.returncode, result.stdout, flush=True)
    return result


def main():
    clang = os.environ.get('NSC_TEST_ASAN_CLANG')
    runtime = os.environ.get('NSC_TEST_ASAN_RUNTIME')
    if os.name != 'nt' or not clang or not runtime:
        print('SKIP: set NSC_TEST_ASAN_CLANG and NSC_TEST_ASAN_RUNTIME on Windows')
        return 77
    unit = Path(sys.argv[1])
    runtime = Path(runtime)
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-asan-integrated-runtime-') as temporary:
        root = Path(temporary)
        for fixture in ('heap-safe', 'heap-bad'):
            folder = root / fixture
            result = run([unit, folder, fixture, clang, '', 'placeholder'], root)
            if result.returncode:
                failures.append(fixture + ': builder test failed')
                continue
            for level in ('O0', 'O2'):
                directory = folder / level
                shutil.copyfile(runtime, directory / runtime.name)
                result = run([directory / 'probe.exe'], directory)
                if fixture == 'heap-safe':
                    if result.returncode != 42 or 'ERROR: AddressSanitizer' in result.stdout:
                        failures.append(fixture + ' ' + level + ': expected 42')
                elif result.returncode == 0 or 'heap-buffer-overflow' not in result.stdout:
                    failures.append(fixture + ' ' + level + ': missing ASan diagnosis')
        # The heap adapter also appears as a standalone object in the builder fixture.
        # Verify the null-on-failure contract without changing ASAN_OPTIONS.
        adapter = root / 'heap-safe' / 'sanitizer-heap.obj'
        failure_ir = Path(__file__).parent / 'asan_heap_out_of_bounds' / 'allocation_failure.ll'
        failure_exe = root / 'allocation-failure.exe'
        result = run([clang, '--target=x86_64-pc-windows-msvc', '-fuse-ld=lld',
                      '-fsanitize=address', failure_ir, adapter, '-o', failure_exe], root)
        if result.returncode:
            failures.append('allocation failure fixture did not link')
        else:
            shutil.copyfile(runtime, root / runtime.name)
            result = run([failure_exe], root)
            if result.returncode != 42 or 'ERROR: AddressSanitizer' in result.stdout:
                failures.append('allocation failure did not preserve null and the original block')
    if failures:
        print('\n'.join(failures), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
