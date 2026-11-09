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
    program = '''using NativeSharp.Core.Internal; using NativeSharp.Core.Threading;
namespace NativeSharp.Core { public class Exception { } }
class Payload { public static int Drops; ~Payload() { Drops = Drops + 1; } }
class Program {
 private static long[] block; private static Payload* slot;
 private static long gate; private static long ready; private static long failures;
 public static void OrderedWorker() {
  if (!SharedAtomicControl.TryRetainStrong(block)) { SharedAtomic.CompareExchange(ref failures, 1l, 0l); return; }
  SharedAtomic.CompareExchange(ref ready, 1l, 0l);
  while (SharedAtomic.Load(ref gate) == 0l) { }
  if (SharedAtomicControl.DropStrong<Payload>(block, slot) != 1 ||
      SharedAtomicControl.TryRetainStrong(block)) SharedAtomic.CompareExchange(ref failures, 1l, 0l);
 }
 public static void RacingWorker() {
  while (SharedAtomic.Load(ref gate) == 0l) { }
  bool expired = false; int index = 0;
  while (index < 64) {
   bool retained = SharedAtomicControl.TryRetainStrong(block);
   if (retained) {
    if (expired) SharedAtomic.CompareExchange(ref failures, 1l, 0l);
    if (SharedAtomicControl.DropStrong<Payload>(block, slot) < 0)
     SharedAtomic.CompareExchange(ref failures, 1l, 0l);
   } else expired = true;
   index = index + 1;
  }
 }
 public static int Main() { unsafe("shared atomic upgrade test") {
  int round = 0; while (round < 128) {
   block = SharedControlStorage.Allocate<Payload>(); Payload value = SharedControlStorage.Construct<Payload>(block);
   slot = (Payload*)&block[6]; *slot = value; gate = 0l; ready = 0l; failures = 0l;
   SharedAtomicControl.Initialize(block); if (!SharedAtomicControl.TryRetainWeak(block)) return 1;
   ThreadStart start = OrderedWorker; raii Thread worker = new Thread(start);
   while (SharedAtomic.Load(ref ready) == 0l && SharedAtomic.Load(ref failures) == 0l) { }
   if (SharedAtomic.Load(ref failures) != 0l || SharedAtomicControl.DropStrong<Payload>(block, slot) != 0) return 2;
   SharedAtomic.CompareExchange(ref gate, 1l, 0l); worker.Join();
   if (SharedAtomic.Load(ref failures) != 0l || SharedAtomicControl.StrongCount(block) != 0l ||
       Payload.Drops != round + 1 || SharedAtomicControl.DropWeak(block) != 1) return 3;
   round = round + 1;
  }
  round = 0; while (round < 128) {
   block = SharedControlStorage.Allocate<Payload>(); Payload value = SharedControlStorage.Construct<Payload>(block);
   slot = (Payload*)&block[6]; *slot = value; gate = 0l; failures = 0l;
   SharedAtomicControl.Initialize(block); if (!SharedAtomicControl.TryRetainWeak(block)) return 4;
   ThreadStart start = RacingWorker; raii Thread worker = new Thread(start);
   SharedAtomic.CompareExchange(ref gate, 1l, 0l);
   if (SharedAtomicControl.DropStrong<Payload>(block, slot) < 0) return 5;
   worker.Join();
   if (SharedAtomic.Load(ref failures) != 0l || SharedAtomicControl.StrongCount(block) != 0l ||
       SharedAtomicControl.TryRetainStrong(block) || Payload.Drops != round + 129 ||
       SharedAtomicControl.DropWeak(block) != 1) return 6;
   round = round + 1;
  } return 42;
 } }
}'''
    with tempfile.TemporaryDirectory(prefix='nsc-shared-upgrade-') as temporary:
        root = Path(temporary)
        for source in sources:
            shutil.copyfile(source, root / source.name)
        (root / 'Main.ns').write_text(program, encoding='utf-8')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        result = run([compiler, '--check-project-oop-hir', project], root)
        assert result.returncode == 0, result.stdout + result.stderr
        if os.name == 'nt':
            for level in ('O0', 'O2'):
                exe = root / (level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-o', exe, '-' + level], root)
                assert result.returncode == 0, (level, result.stdout + result.stderr)
                result = run([exe], root)
                assert result.returncode == 42, (level, result.returncode, result.stdout, result.stderr)
                print(level, 'exit=42', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
