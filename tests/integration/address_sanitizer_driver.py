import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, env, cwd):
    result = subprocess.run([str(x) for x in command], cwd=cwd, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace', timeout=120)
    if result.returncode:
        print('exit', result.returncode, 'for', command, '\n', result.stdout, flush=True)
    return result


def main():
    clang = os.environ.get('NSC_TEST_ASAN_CLANG')
    runtime = os.environ.get('NSC_TEST_ASAN_RUNTIME')
    if os.name != 'nt' or not clang or not runtime:
        print('SKIP: set NSC_TEST_ASAN_CLANG and NSC_TEST_ASAN_RUNTIME on Windows')
        return 77
    compiler = Path(sys.argv[1])
    clang, runtime = Path(clang), Path(runtime)
    env = os.environ.copy()
    # The generated executable must find the packaged DLL without a PATH override.
    runtime_dir = str(runtime.parent).lower()
    env['PATH'] = os.pathsep.join(p for p in env.get('PATH', '').split(os.pathsep)
                                  if p.lower().rstrip('\\/') != runtime_dir.rstrip('\\/'))
    with tempfile.TemporaryDirectory(prefix='nsc asan regression ') as tmp:
        root = Path(tmp)
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
                flags = ['--sanitize-address', '--asan-clang=' + str(clang), '--asan-runtime=' + str(runtime)]
                result = run(base + flags, env, folder)
                assert result.returncode == 0, 'sanitized build failed'
                packaged = folder / runtime.name
                assert packaged.is_file() and hashlib.sha256(packaged.read_bytes()).digest() == hashlib.sha256(runtime.read_bytes()).digest()
                result = run(base + flags, env, folder)
                assert result.returncode == 0, 'rebuilding an existing sanitized output failed'
                result = run([exe], env, folder)
                if case == 'safe':
                    assert result.returncode == 42 and 'ERROR: AddressSanitizer' not in result.stdout
                    plain = folder / 'plain.exe'
                    result = run([compiler, '--emit-project-oop-exe', folder / 'Probe.nsproj', '-o', plain,
                                  '-' + level, '--no-artifact-cache', '--no-banner'], env, folder)
                    assert result.returncode == 0
                    assert b'clang_rt.asan_dynamic' not in plain.read_bytes()
                    assert run([plain], env, folder).returncode == 42
                else:
                    assert result.returncode != 0 and 'dynamic-stack-buffer-overflow' in result.stdout
                for bad_flag in ('-flto=thin', '--incremental-link', '-g', '--artifact-cache=' + str(folder / 'cache')):
                    rejected = run(base + flags + [bad_flag], env, folder)
                    assert rejected.returncode != 0 and '--sanitize-address' in rejected.stdout
                missing = run(base + ['--sanitize-address'], env, folder)
                assert missing.returncode != 0 and '--asan-clang' in missing.stdout
                missing_runtime = run(base + ['--sanitize-address', '--asan-clang=' + str(clang),
                                              '--asan-runtime=' + str(folder / 'missing.dll')], env, folder)
                assert missing_runtime.returncode != 0 and '--asan-runtime' in missing_runtime.stdout
    return 0


if __name__ == '__main__':
    sys.exit(main())
