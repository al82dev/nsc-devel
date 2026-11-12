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
    repo = Path(__file__).resolve().parents[2]
    asan_flags = []
    if os.environ.get('NSC_SHARED_CONSTRUCTOR_CLEANUP_ASAN') == '1':
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
class Trace { public static int PayloadDrops; public static int ResourceDrops; }
class Resource { ~Resource() { Trace.ResourceDrops = Trace.ResourceDrops + 1; } }
class GateBefore { static GateBefore() throws Exception { throw new Exception(); }
 public static int Read() { return 1; } }
class GateAfter { static GateAfter() throws Exception { throw new Exception(); }
 public static int Read() { return 1; } }
KIND Early { private owned Resource item;
 public Early() { int ignored = GateBefore.Read(); item = new Resource(); }
 ~Early() { Trace.PayloadDrops = Trace.PayloadDrops + 1; } }
KIND Late { private owned Resource item;
 public Late() { item = new Resource(); int ignored = GateAfter.Read(); }
 ~Late() { Trace.PayloadDrops = Trace.PayloadDrops + 1; } }
KIND Good { private owned Resource item;
 public Good() { item = new Resource(); }
 ~Good() { Trace.PayloadDrops = Trace.PayloadDrops + 1; } }
class Program { public static int Main() { int caught = 0;
 try { SharedFAMILYHandle<Early> early = SharedFAMILYFactory.Create<Early>(); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 1 || Trace.PayloadDrops != 0 || Trace.ResourceDrops != 0) return 1;
 try { SharedFAMILYHandle<Late> late = SharedFAMILYFactory.Create<Late>(); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 2 || Trace.PayloadDrops != 0 || Trace.ResourceDrops != 1) return 2;
 { SharedFAMILYHandle<Good> good = SharedFAMILYFactory.Create<Good>(); }
 return Trace.PayloadDrops == 1 && Trace.ResourceDrops == 2 ? 42 : 3;
} }
'''
    with tempfile.TemporaryDirectory(prefix='nsc-shared-implicit-cleanup-') as temporary:
        root = Path(temporary)
        for source in sources:
            shutil.copyfile(source, root / source.name)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        for family in ('Local', 'Atomic'):
            for kind in ('class', 'struct'):
                (root / 'Main.ns').write_text(program.replace('FAMILY', family).replace('KIND', kind),
                                               encoding='utf-8')
                if os.name == 'nt':
                    for level in ('O0', 'O2'):
                        exe = root / (family + '-' + kind + '-' + level + '.exe')
                        result = run([compiler, '--emit-project-oop-exe', project, '-o', exe, '-' + level,
                                      '--no-artifact-cache', '--no-error-log'] + asan_flags, repo)
                        assert result.returncode == 0, (family, kind, level, result.stdout + result.stderr)
                        result = run([exe], repo)
                        assert result.returncode == 42 and 'ERROR: AddressSanitizer' not in result.stdout + result.stderr, (
                            family, kind, level, result.returncode, result.stdout, result.stderr)
                        print(family, kind, level, 'exit=42', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
