"""File operations on a path with non-ASCII characters (NSC-NEW-019).

GetSize, ReadAllBytes, ReadBytes, WriteAllBytes, WriteAllText and AppendAllText opened the file with
CreateFileA, so a UTF-8 path with 'ñ' named a different file in the ANSI code page: GetSize returned -1
while Exists and ReadAllText found the file. They use CreateFileW now. The program writes, appends,
measures and reads back a file in a directory whose name is not ASCII, at O0 and O2.
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

SOURCE = r'''using NativeSharp.Core;
using NativeSharp.Core.IO;
class Program { public static int Main() {
 string[] a = new string[2]; if (Environment.GetCommandLineArgs(a) != 2) { return 1; }
 raii string path = Path.Combine(a[1], "datos.txt");
 File.WriteAllText(path, "hola");
 File.AppendAllText(path, " ñandú");
 if (!File.Exists(path)) { return 2; }
 if (File.GetSize(path) != 12) { return 3; }
 byte[] bytes = File.ReadAllBytes(path); if (bytes.Length != 12) { return 4; }
 byte[] buffer = new byte[4]; if (File.ReadBytes(path, buffer) != 4 || buffer[0] != (byte)'h') { return 5; }
 raii string binary = Path.Combine(a[1], "binario.bin");
 byte[] payload = new byte[3]; payload[0] = (byte)7; payload[1] = (byte)8; payload[2] = (byte)9;
 if (!File.WriteAllBytes(binary, payload, 3)) { return 8; }
 if (File.GetSize(binary) != 3) { return 6; }
 string text = File.ReadAllText(path); if (text.Length != 12) { return 7; }
 return 42; } }
'''


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    repo = Path(__file__).resolve().parent.parent.parent
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-unicode-paths-') as temporary:
        root = Path(temporary)
        os.environ['TEMP'] = os.environ['TMP'] = temporary
        libraries = root / 'libraries'
        libraries.mkdir()
        for name in ('NativeSharp.Core', 'NativeSharp.Core.IO'):
            shutil.copytree(repo / 'lib' / name, libraries / name, ignore=shutil.ignore_patterns('obj', 'bin'))
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><OutputType>Exe</OutputType></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" />'
                           '<ProjectReference Include="libraries/NativeSharp.Core/NativeSharp.Core.nsproj" />'
                           '<ProjectReference Include="libraries/NativeSharp.Core.IO/NativeSharp.Core.IO.nsproj" />'
                           '</ItemGroup></Project>', encoding='utf-8')
        (root / 'Main.ns').write_text(SOURCE, encoding='utf-8')
        target = root / 'carpeta ñandú'
        target.mkdir()
        for level in ('O0', 'O2'):
            exe = root / ('probe-' + level + '.exe')
            build = subprocess.run([compiler, '-' + level, '--emit-project-oop-exe', str(project), '-o', str(exe),
                                    '--no-artifact-cache', '--no-error-log'], cwd=root, capture_output=True,
                                   text=True, errors='replace', timeout=300)
            if build.returncode:
                failures.append((level, 'build', (build.stdout + build.stderr)[-400:]))
                continue
            result = subprocess.run([str(exe), str(target)], cwd=root, capture_output=True, timeout=60)
            print(level, 'exit', result.returncode, flush=True)
            if result.returncode != 42:
                failures.append((level, 'exit', result.returncode))
            if (target / 'datos.txt').read_bytes() != 'hola ñandú'.encode('utf-8'):
                failures.append((level, 'content', (target / 'datos.txt').read_bytes()))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('unicode_file_paths: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
