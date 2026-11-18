"""Check successful growth and shrink through a freshly generated ASan heap adapter."""

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def digest(path):
    """Identify a file before and after the directed run without modifying it."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, cwd, env):
    """Run one bounded process, reporting its cwd, output and progress."""
    arguments = [str(part) for part in command]
    print('COMMAND', subprocess.list2cmdline(arguments), 'CWD', cwd, flush=True)
    process = subprocess.Popen(arguments, cwd=cwd, env=env, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, errors='replace')
    deadline = time.monotonic() + 120
    while True:
        try:
            output, _ = process.communicate(timeout=min(25, max(0, deadline - time.monotonic())))
            break
        except subprocess.TimeoutExpired:
            if time.monotonic() >= deadline:
                process.kill()
                output, _ = process.communicate()
                print('TIMEOUT after 120 seconds', output, flush=True)
                return None, output
            print('RUNNING', arguments[0], flush=True)
    print('EXIT', process.returncode, output, flush=True)
    return process.returncode, output


def arguments():
    """Require the actual compiler, Clang, ASan runtime and independent temp root."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--clang', type=Path, required=True)
    parser.add_argument('--asan-runtime', type=Path, required=True)
    parser.add_argument('--temp-root', type=Path, required=True)
    return parser.parse_args()


def main():
    """Generate adapters in O0/O2, then verify valid realloc preserves data."""
    args = arguments()
    repo = Path(__file__).resolve().parents[2]
    fixture = repo / 'tests' / 'integration' / 'asan_heap_out_of_bounds' / 'realloc_preserve.ll'
    runtime = os.environ.get('NSC_WINDOWS_RUNTIME_LIBRARY')
    imports = os.environ.get('NSC_WINDOWS_RUNTIME_IMPORT_LIBRARY_DIRECTORY')
    if not runtime or not imports:
        print('ERROR: NSC_WINDOWS_RUNTIME_LIBRARY and import library directory are required', flush=True)
        return 2
    imports_path = Path(imports)
    libraries = [imports_path / (name + '.import.lib')
                 for name in ('KERNEL32', 'KERNELBASE', 'SHELL32', 'UCRTBASE')]
    inputs = [args.compiler, args.clang, args.asan_runtime, Path(runtime), fixture, *libraries]
    if not args.temp_root.is_dir() or not Path(imports).is_dir() or any(not item.is_file() for item in inputs):
        print('ERROR: a required input or directory does not exist', flush=True)
        return 2
    before = {str(item.resolve()): digest(item) for item in inputs}
    print('SHA256 BEFORE', before, 'ASAN_OPTIONS', os.environ.get('ASAN_OPTIONS', '<not set>'), flush=True)
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-asan-realloc-preserve-', dir=args.temp_root) as temporary:
        root = Path(temporary)
        (root / 'Main.ns').write_text('int Main() { return 42; }\n', encoding='utf-8')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>AsanRealloc</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        shutil.copyfile(args.asan_runtime, root / args.asan_runtime.name)
        for level in ('O0', 'O2'):
            env = os.environ.copy()
            env['NSC_KEEP_LINK_TEMPS'] = str(root / ('link-' + level))
            producer = root / ('producer-' + level + '.exe')
            command = [args.compiler.resolve(), '--emit-project-oop-exe', project, '-' + level,
                       '-o', producer, '--no-artifact-cache', '--no-error-log', '--sanitize-address',
                       '--asan-clang=' + str(args.clang.resolve()), '--asan-runtime=' + str(args.asan_runtime.resolve())]
            code, _ = run(command, repo, env)
            if code != 0:
                failures.append(level + ': cannot generate adapter')
                continue
            adapters = list(Path(env['NSC_KEEP_LINK_TEMPS']).rglob('sanitizer-heap.obj'))
            if len(adapters) != 1:
                failures.append(level + ': expected one generated adapter')
                continue
            adapter = adapters[0]
            print('ADAPTER', level, 'SHA256', digest(adapter), flush=True)
            executable = root / ('realloc-' + level + '.exe')
            command = [args.clang.resolve(), '--target=x86_64-pc-windows-msvc', '-fuse-ld=lld',
                       '-fsanitize=address', '-' + level, fixture, adapter, '-o', executable]
            code, _ = run(command, repo, env)
            if code != 0:
                failures.append(level + ': cannot link fixture')
                continue
            code, output = run([executable], repo, env)
            if code != 42 or 'ERROR: AddressSanitizer' in output:
                failures.append(level + ': realloc data preservation failed')
    after = {str(item.resolve()): digest(item) for item in inputs}
    print('SHA256 AFTER', after, flush=True)
    if before != after:
        failures.append('an input changed during the test')
    if failures:
        print('FAIL:', '; '.join(failures), flush=True)
        return 1
    print('PASS: alloc, grow, preserve, shrink, preserve and free in O0/O2', flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
