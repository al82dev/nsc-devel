import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(item) for item in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler, *sources = map(Path, sys.argv[1:])
    asan_flags = []
    if os.environ.get('NSC_SHARED_CLEANUP_ASAN') == '1':
        clang = os.environ.get('NSC_TEST_ASAN_CLANG')
        runtime = os.environ.get('NSC_TEST_ASAN_RUNTIME')
        if not clang or not runtime:
            print('SKIP: set NSC_TEST_ASAN_CLANG and NSC_TEST_ASAN_RUNTIME', flush=True)
            return 77
        asan_flags = ['--sanitize-address', '--asan-clang=' + clang, '--asan-runtime=' + runtime]
        options = os.environ.get('ASAN_OPTIONS', '')
        os.environ['ASAN_OPTIONS'] = options + (':' if options else '') + 'detect_leaks=0'
    program = '''using NativeSharp.Core; using NativeSharp.Core.Internal;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int Drops; }
class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 1; } }
KIND Payload { ~Payload() { Trace.Drops = Trace.Drops + 1; int ignored = Gate.Read(); } }
class Program { public static int Main() { unsafe("shared destructor exception") {
 STORAGE
 SharedFAMILYControl.Initialize(block);
 if (!SharedFAMILYControl.TryRetainWeak(block)) return 1;
 int caught = 0;
 try { SharedFAMILYControl.DropStrong<Payload>(block, slot); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 1 || Trace.Drops != 1 || SharedFAMILYControl.StrongCount(block) != 0l) return 2;
 if (SharedFAMILYControl.TryRetainStrong(block) || SharedFAMILYControl.WeakCount(block) != 1l) return 3;
 return SharedFAMILYControl.DropWeak(block) == 1 ? 42 : 4;
} } }
'''
    storage = {
        'class': '''long[] block = SharedControlStorage.Allocate<Payload>();
 Payload value = SharedControlStorage.Construct<Payload>(block);
 Payload* slot = (Payload*)&block[6]; *slot = value;''',
        'struct': '''long[] block = SharedControlStorage.AllocateValue<Payload>();
 Payload* slot = SharedControlStorage.ConstructValue<Payload>(block);''',
    }
    with tempfile.TemporaryDirectory(prefix='nsc-shared-destructor-cleanup-') as temporary:
        root = Path(temporary)
        for source in sources:
            shutil.copyfile(source, root / source.name)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        for family in ('Local', 'Atomic'):
            for kind in ('class', 'struct'):
                source = program.replace('FAMILY', family).replace('KIND', kind).replace('STORAGE', storage[kind])
                (root / 'Main.ns').write_text(source, encoding='utf-8')
                if os.name == 'nt':
                    for level in ('O0', 'O2'):
                        exe = root / (family + '-' + kind + '-' + level + '.exe')
                        result = run([compiler, '--emit-project-oop-exe', project, '-o', exe, '-' + level,
                                      '--no-artifact-cache', '--no-error-log'] + asan_flags, root)
                        assert result.returncode == 0, (family, kind, level, result.stdout + result.stderr)
                        result = run([exe], root)
                        assert result.returncode == 42 and 'ERROR: AddressSanitizer' not in result.stdout + result.stderr, (
                            family, kind, level, result.returncode, result.stdout, result.stderr)
                        print(family, kind, level, 'exit=42', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
