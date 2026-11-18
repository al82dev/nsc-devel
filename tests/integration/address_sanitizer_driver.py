import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def report(message):
    print(message, flush=True)
    log = os.environ.get('NSC_DIRECTED_LOG')
    if log:
        with Path(log).open('a', encoding='utf-8') as output:
            output.write(message + '\n')


def run(command, env, cwd):
    arguments = [str(x) for x in command]
    report('COMMAND ' + subprocess.list2cmdline(arguments))
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
                report('TIMEOUT 120 seconds\n' + output)
                raise
            report('RUNNING ' + arguments[0])
    report('EXIT ' + str(process.returncode) + '\n' + output)
    return subprocess.CompletedProcess(arguments, process.returncode, output)


def main():
    log = os.environ.get('NSC_DIRECTED_LOG')
    if log:
        Path(log).open('x', encoding='utf-8').close()
    report('ASAN_OPTIONS=' + os.environ.get('ASAN_OPTIONS', '<not set>'))
    report('Core: fixture does not declare or verify an instrumented Core binary')
    clang = os.environ.get('NSC_TEST_ASAN_CLANG')
    runtime = os.environ.get('NSC_TEST_ASAN_RUNTIME')
    if os.name != 'nt' or not clang or not runtime:
        report('SKIP: set NSC_TEST_ASAN_CLANG and NSC_TEST_ASAN_RUNTIME on Windows')
        return 77
    compiler = Path(sys.argv[1])
    clang, runtime = Path(clang), Path(runtime)
    env = os.environ.copy()
    # Test packaged DLL lookup only when the inherited PATH cannot supply it.
    runtime_dir = str(runtime.parent).lower()
    if any(p.lower().rstrip('\\/') == runtime_dir.rstrip('\\/')
           for p in env.get('PATH', '').split(os.pathsep)):
        report('SKIP: ASan runtime directory is already in the inherited PATH')
        return 77
    repository = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix='nsc asan regression ') as tmp:
        root = Path(tmp)
        wrong_dir = root / 'wrong-runtime'
        wrong_dir.mkdir()
        wrong_runtime = wrong_dir / runtime.name
        wrong_bytes = bytearray(runtime.read_bytes())
        wrong_bytes[-1] ^= 1
        wrong_runtime.write_bytes(wrong_bytes)
        copied_dir = root / 'matching-runtime'
        copied_dir.mkdir()
        copied_runtime = copied_dir / runtime.name
        copied_runtime.write_bytes(runtime.read_bytes())
        for case in ('safe', 'bad'):
            for level in ('O0', 'O2'):
                folder = root / (case + '-' + level)
                folder.mkdir()
                (folder / 'Probe.nsproj').write_text(
                    '<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                    '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
                body = 'p[0] = 42; return p[0];' if case == 'safe' else 'int i = 3; p[i] = 42; return p[0];'
                (folder / 'Main.ns').write_text('int Main() { int* p = stackalloc int[2]; ' + body + ' }', encoding='utf-8')
                exe = folder / 'probe.exe'
                base = [compiler, '--emit-project-oop-exe', folder / 'Probe.nsproj', '-o', exe,
                        '-' + level, '--no-banner']
                selected_runtime = copied_runtime if (case, level) == ('safe', 'O0') else runtime
                flags = ['--sanitize-address', '--asan-clang=' + str(clang), '--asan-runtime=' + str(selected_runtime)]
                result = run(base + flags, env, repository)
                assert result.returncode == 0, 'sanitized build failed'
                packaged = folder / runtime.name
                assert packaged.is_file() and hashlib.sha256(packaged.read_bytes()).digest() == hashlib.sha256(runtime.read_bytes()).digest()
                result = run(base + flags, env, repository)
                assert result.returncode == 0, 'rebuilding an existing sanitized output failed'
                result = run([exe], env, repository)
                if case == 'safe':
                    assert result.returncode == 42 and 'ERROR: AddressSanitizer' not in result.stdout
                    plain = folder / 'plain.exe'
                    result = run([compiler, '--emit-project-oop-exe', folder / 'Probe.nsproj', '-o', plain,
                                  '-' + level, '--no-artifact-cache', '--no-banner'], env, repository)
                    assert result.returncode == 0
                    assert b'clang_rt.asan_dynamic' not in plain.read_bytes()
                    assert run([plain], env, repository).returncode == 42
                else:
                    assert result.returncode != 0 and 'dynamic-stack-buffer-overflow' in result.stdout
                for bad_flag in ('-flto=thin', '--incremental-link', '-g', '--artifact-cache=' + str(folder / 'cache')):
                    rejected = run(base + flags + [bad_flag], env, repository)
                    assert rejected.returncode != 0 and '--sanitize-address' in rejected.stdout
                missing = run(base + ['--sanitize-address'], env, repository)
                assert missing.returncode != 0 and '--asan-clang' in missing.stdout
                missing_runtime = run(base + ['--sanitize-address', '--asan-clang=' + str(clang),
                                              '--asan-runtime=' + str(folder / 'missing.dll')], env, repository)
                assert missing_runtime.returncode != 0 and '--asan-runtime' in missing_runtime.stdout
                mismatch = run(base + ['--sanitize-address', '--asan-clang=' + str(clang),
                                        '--asan-runtime=' + str(wrong_runtime)], env, repository)
                assert mismatch.returncode != 0 and 'runtime does not match' in mismatch.stdout
    report('RESULT PASS; omitted=0')
    return 0


if __name__ == '__main__':
    sys.exit(main())
