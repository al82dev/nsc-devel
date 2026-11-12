"""Check that the current ASan heap adapter rejects incompatible inputs."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    clang = os.environ.get('NSC_TEST_ASAN_CLANG')
    runtime = os.environ.get('NSC_TEST_ASAN_RUNTIME')
    if os.name != 'nt' or not clang or not runtime:
        print('SKIP: set NSC_TEST_ASAN_CLANG and NSC_TEST_ASAN_RUNTIME on Windows')
        return 77
    compiler = Path(sys.argv[1])
    repo = Path(__file__).resolve().parents[2]
    fixtures = repo / 'tests' / 'integration' / 'asan_incompatible_heap'
    env = os.environ.copy()
    options = env.get('ASAN_OPTIONS', '')
    env['ASAN_OPTIONS'] = options + (':' if options else '') + 'detect_leaks=0'
    with tempfile.TemporaryDirectory(prefix='nsc-asan-incompatible-heap-') as temporary:
        root = Path(temporary)
        env['NSC_KEEP_LINK_TEMPS'] = str(root / 'link-temps')
        (root / 'Main.ns').write_text(
            'int Main() { int[] v = new int[1]; v[0] = 42; int r = v[0]; delete[] v; return r; }\n',
            encoding='utf-8')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>AsanHeapProbe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')

        def run(command):
            command = [str(part) for part in command]
            result = subprocess.run(command, cwd=repo, env=env, capture_output=True,
                                    text=True, errors='replace', timeout=120)
            print('COMMAND', subprocess.list2cmdline(command), 'CWD', repo, 'EXIT', result.returncode,
                  result.stdout + result.stderr, flush=True)
            return result

        executable = root / 'producer.exe'
        build = run([compiler, '--emit-project-oop-exe', project, '-O0', '-o', executable,
                     '--no-artifact-cache', '--no-error-log', '--sanitize-address',
                     '--asan-clang=' + clang, '--asan-runtime=' + runtime])
        if build.returncode:
            return 1
        adapters = list((root / 'link-temps').rglob('sanitizer-heap.obj'))
        if len(adapters) != 1:
            print('expected one generated adapter, found', len(adapters))
            return 1
        adapter = adapters[0]
        print('adapter SHA-256', hashlib.sha256(adapter.read_bytes()).hexdigest(), flush=True)
        shutil.copyfile(runtime, root / Path(runtime).name)
        for name in ('foreign-free', 'foreign-realloc', 'foreign-heap',
                     'realloc-null', 'realloc-zero', 'allocation-flags'):
            exe = root / (name + '.exe')
            result = run([clang, '--target=x86_64-pc-windows-msvc', '-fuse-ld=lld',
                          '-fsanitize=address', fixtures / (name + '.ll'), adapter, '-o', exe])
            if result.returncode:
                return 1
            result = run([exe])
            if result.returncode != 87 or 'NativeSharp ASan: unsupported heap, flags, allocation, or realloc arguments' not in result.stdout + result.stderr:
                return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
