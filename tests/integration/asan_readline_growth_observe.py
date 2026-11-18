"""Targeted Console.ReadLine growth check, with and without ASan.

This does not establish that realloc moved, nor prove general heap safety.
"""
from pathlib import Path
from xml.sax.saxutils import escape
import hashlib
import os
import subprocess
import sys
import tempfile


def main():
    repo = Path(__file__).resolve().parents[2]
    compiler = Path(sys.argv[1]).resolve()
    names = ('NSC_TEST_CORE_BINARY', 'NSC_TEST_ASAN_CLANG', 'NSC_TEST_ASAN_RUNTIME',
             'NSC_WINDOWS_RUNTIME_LIBRARY', 'NSC_WINDOWS_RUNTIME_IMPORT_LIBRARY_DIRECTORY')
    missing = [name for name in names if not os.environ.get(name)]
    if missing:
        print('SKIP: missing optional configuration', ', '.join(missing), flush=True)
        return 77
    core, clang, asan_runtime, runtime_library, import_directory = (
        Path(os.environ[name]).resolve() for name in names)
    root_name = os.environ.get('NSC_TEST_TEMP_ROOT') or os.environ.get('TEMP')
    if not root_name or Path(root_name).resolve().drive.upper() != 'A:' or not Path(root_name).is_dir():
        print('FAIL: temporary root must exist on A:', flush=True)
        return 1
    metadata = core.with_suffix('.nsmeta')
    inputs = [compiler, core, metadata, clang, asan_runtime, runtime_library]
    if any(not path.is_file() for path in inputs) or not import_directory.is_dir():
        print('FAIL: configured compiler, Core, metadata, clang, runtime or import directory is invalid', flush=True)
        return 1
    imports = sorted(import_directory.rglob('*.lib'))
    if not imports:
        print('FAIL: configured import directory has no libraries', flush=True)
        return 1
    inputs.extend(imports)
    def hashes():
        return {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    before = hashes()
    print('SHA256 BEFORE', before, flush=True)
    root = Path(tempfile.mkdtemp(prefix='nsc-asan-readline-', dir=root_name))
    env = os.environ.copy()
    for key in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'):
        env[key] = str(root)
    env['NSC_LINKER'] = 'nslink.exe'
    env['ASAN_OPTIONS'] = 'detect_leaks=0'
    print('EVIDENCE', root, 'CWD', repo, 'ASAN_OPTIONS', env['ASAN_OPTIONS'], flush=True)
    (root / 'Main.ns').write_text('''using NativeSharp.Core;
public class Program { public static int Main() {
 int status = 0; raii string line = Console.ReadLine(out status);
 if (status != 0 || line == null || line.Length != 8192) return 1;
 for (int i = 0; i < 8192; i++) {
   if (i == 0) { if (line[i] != 'a') return 2; }
   else if (i == 4096) { if (line[i] != 'b') return 3; }
   else if (i == 8191) { if (line[i] != 'z') return 4; }
   else { if (line[i] != 'x') return 5; }
 }
 return 42; } }
''', encoding='utf-8')
    ref = lambda p: escape(p.resolve().as_posix(), {'"': '&quot;'})
    (root / 'Main.nsproj').write_text(
        '<Project><PropertyGroup><Name>ReadLineGrowth</Name><OutputType>Exe</OutputType>'
        '</PropertyGroup><ItemGroup><Compile Include="Main.ns"/><LibraryReference Include="' +
        ref(core) + '" Metadata="' + ref(metadata) + '"/></ItemGroup></Project>', encoding='utf-8')
    data = b'a' + b'x' * 4095 + b'b' + b'x' * 4094 + b'z\n'
    failures = []
    for mode in ('normal', 'asan'):
        for level in ('O0', 'O2'):
            output = root / (mode + '-' + level + '.exe')
            cmd = [str(compiler), '--emit-project-oop-exe', str(root / 'Main.nsproj'),
                   '-' + level, '-o', str(output), '--no-artifact-cache']
            if mode == 'asan':
                cmd += ['--sanitize-address', '--asan-clang=' + str(clang),
                        '--asan-runtime=' + str(asan_runtime)]
            build = subprocess.run(cmd, cwd=repo, env=env, capture_output=True, text=True, errors='replace', timeout=180)
            print('BUILD', mode, level, subprocess.list2cmdline(cmd), 'EXIT', build.returncode,
                  build.stdout + build.stderr, flush=True)
            if build.returncode != 0 or not output.is_file():
                failures.append((mode, level, 'build'))
                continue
            result = subprocess.run([str(output)], cwd=repo, env=env, input=data,
                                    capture_output=True, timeout=120)
            text = (result.stdout + result.stderr).decode(errors='replace')
            print('RUN', mode, level, 'EXIT', result.returncode, text, flush=True)
            if result.returncode != 42 or 'ERROR: AddressSanitizer' in text:
                failures.append((mode, level, 'run'))
    after = hashes()
    if before != after:
        failures.append(('input-hash-changed',))
    print('SHA256 AFTER', after, 'FAILURES', failures, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
