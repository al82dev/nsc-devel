"""Check public strong and weak shared families under AddressSanitizer."""

from pathlib import Path
import os
import subprocess
import sys

from isolated_a_runner import absolute_a, setup


FILES = ('SharedStorage.ns', 'SharedControlStorage.ns', 'SharedReferenceCounts.ns',
         'SharedLocalControl.ns', 'SharedAtomicControl.ns', 'SharedBorrow.ns',
         'SharedLocalHandle.ns', 'SharedLocalWeak.ns', 'SharedLocalFactory.ns',
         'SharedAtomicHandle.ns', 'SharedAtomicWeak.ns', 'SharedAtomicFactory.ns',
         'SharedView.ns', 'Rc.ns', 'WeakRc.ns', 'Arc.ns', 'WeakArc.ns', 'SharedPointers.ns',
         'Send.ns', 'Sync.ns')

PROGRAM = '''using NativeSharp.Core;
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
 try { STRONG<Early> early = SharedPointers.MAKETHROW<Early>(); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 1 || Trace.PayloadDrops != 0 || Trace.ResourceDrops != 0) return 1;
 try { STRONG<Late> late = SharedPointers.MAKETHROW<Late>(); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 2 || Trace.PayloadDrops != 0 || Trace.ResourceDrops != 1) return 2;
 WEAK<Good> weak;
 { STRONG<Good> good = SharedPointers.MAKE<Good>(); weak = good.Downgrade();
   if (weak.Expired() || good.StrongCount() != 1 || good.WeakCount() != 1) return 3;
   { STRONG<Good> clone = weak.Upgrade(); if (clone.IsEmpty() || good.StrongCount() != 2) return 4; }
 }
 if (!weak.Expired()) return 5;
 { STRONG<Good> expired = weak.Upgrade(); if (!expired.IsEmpty()) return 6; }
 return Trace.PayloadDrops == 1 && Trace.ResourceDrops == 2 ? 42 : 7;
} }
'''


def main():
    if len(sys.argv) != 3:
        raise SystemExit('expected nsc and a new evidence directory on A:')
    clang = os.environ.get('NSC_TEST_ASAN_CLANG')
    runtime = os.environ.get('NSC_TEST_ASAN_RUNTIME')
    if not clang or not runtime:
        print('SKIP: set NSC_TEST_ASAN_CLANG and NSC_TEST_ASAN_RUNTIME', flush=True)
        return 77
    compiler, root = (absolute_a(value) for value in sys.argv[1:])
    repo = Path(__file__).resolve().parents[2]
    environment = setup(root, repo)
    environment['ASAN_OPTIONS'] = 'detect_leaks=0'
    core = repo / 'lib' / 'NativeSharp.Core'
    for name in FILES:
        (root / name).write_bytes((core / name).read_bytes())
    project = root / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                       '<ItemGroup><Compile Include="*.ns"/></ItemGroup></Project>', encoding='utf-8')
    failures = []
    for strong, weak in (('Rc', 'WeakRc'), ('Arc', 'WeakArc')):
        for kind in ('class', 'struct'):
            source = PROGRAM.replace('KIND', kind).replace('STRONG', strong).replace('WEAK', weak)
            source = source.replace('MAKETHROW', 'Make' + strong + 'Throwing').replace('MAKE', 'Make' + strong)
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / (strong + '-' + kind + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level, '-o', str(exe),
                           '--no-artifact-cache', '--sanitize-address', '--asan-clang=' + clang,
                           '--asan-runtime=' + runtime]
                print('COMMAND', subprocess.list2cmdline(command), flush=True)
                result = subprocess.run(command, cwd=repo, env=environment, capture_output=True,
                                        text=True, errors='replace', timeout=120)
                if result.returncode:
                    failures.append((strong, kind, level, 'compile', result.stdout + result.stderr))
                    continue
                result = subprocess.run([str(exe)], cwd=repo, env=environment, capture_output=True,
                                        text=True, errors='replace', timeout=30)
                print('RUN', strong, kind, level, result.returncode, flush=True)
                if result.returncode != 42 or 'ERROR: AddressSanitizer' in result.stdout + result.stderr:
                    failures.append((strong, kind, level, 'run', result.returncode, result.stdout + result.stderr))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
