"""Use ASan to verify array storage is released after an element destructor throws."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    clang = os.environ.get('NSC_TEST_ASAN_CLANG')
    runtime = os.environ.get('NSC_TEST_ASAN_RUNTIME')
    if os.name != 'nt' or not clang or not runtime:
        print('SKIP: set NSC_TEST_ASAN_CLANG and NSC_TEST_ASAN_RUNTIME')
        return 77
    compiler = Path(sys.argv[1]).resolve()
    repo = Path(__file__).resolve().parents[2]
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-asan-throwing-array-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>ThrowingArray</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
        for kind in ('struct', 'class'):
            field_address = ('&value.values[0].Id' if kind == 'struct' else
                             '(int*)&value.values[0]')
            body = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int Log; public static int* Address; }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
KIND Part { public int Id; public Part(int id) { Id = id; }
 ~Part() { if (Id != 0) { Trace.Log = Trace.Log * 10 + Id; if (Id == THROW_ID) { int ignored = Gate.Read(); } } } }
class Payload { public owned Part[] values;
 public Payload() { values = new Part[2]; values[0] = new Part(1); values[1] = new Part(2); } }
class Program { public static int Main() { unsafe("verify storage release under ASan") {
 int caught = 0; try { Payload value = new Payload(); Trace.Address = ADDRESS;
 if (*Trace.Address == -1) return 8; }
 catch (Exception error) { caught = 1; }
 if (caught != 1 || Trace.Log != EXPECTED_LOG) return 9;
 return *Trace.Address;
} } }
'''.replace('KIND', kind).replace('ADDRESS', field_address).replace(
                'THROW_ID', '2' if kind == 'struct' else '1').replace(
                'EXPECTED_LOG', '21' if kind == 'struct' else '12')
            (root / 'Main.ns').write_text(body, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (kind + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache', '--sanitize-address',
                           '--asan-clang=' + clang, '--asan-runtime=' + runtime]
                result = subprocess.run(command, cwd=repo, capture_output=True, text=True,
                                        errors='replace', timeout=120)
                print(kind, level, 'COMMAND', subprocess.list2cmdline(command),
                      'CWD', repo, 'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
                if result.returncode:
                    failures.append((kind, level, 'compile', result.returncode))
                    continue
                result = subprocess.run([str(executable)], cwd=repo, capture_output=True, text=True,
                                        errors='replace', timeout=120)
                output = result.stdout + result.stderr
                print(kind, level, 'RUN CWD', repo, 'EXIT', result.returncode, output, flush=True)
                if result.returncode == 0 or 'heap-use-after-free' not in output:
                    failures.append((kind, level, 'missing ASan use-after-free', result.returncode))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('asan_throwing_array_release: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
