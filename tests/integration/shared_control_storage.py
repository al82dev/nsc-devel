import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(value) for value in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler, storage_source, control_source, counts_source, thread_source = map(Path, sys.argv[1:6])
    source = '''using NativeSharp.Core.Internal;
using NativeSharp.Core.Threading;
class Payload { public static int Drops; public align(32) long A; public long B; ~Payload() { Drops = Drops + 1; } }
class Program {
 private static COUNTS* globalCounts; private static long failures; private static long ready; private static long last;
 public static void Work() { unsafe("shared counts") { int i = 0; while (i < 50000) {
  if (!(*globalCounts).TryRetainStrong()) SharedAtomic.CompareExchange(ref failures, 1l, 0l);
  else if ((*globalCounts).ReleaseStrong() != 0) SharedAtomic.CompareExchange(ref failures, 1l, 0l);
  i = i + 1;
 } } }
 public static void RecordRelease(int result) {
  if (result == 1) { if (SharedAtomic.CompareExchange(ref last, 1l, 0l) != 0l) SharedAtomic.CompareExchange(ref failures, 1l, 0l); }
  else if (result != 0) SharedAtomic.CompareExchange(ref failures, 1l, 0l);
 }
 public static void RaceWork() { unsafe("shared counts") {
  SharedAtomic.CompareExchange(ref ready, 1l, 0l);
  if ((*globalCounts).TryRetainStrong()) RecordRelease((*globalCounts).ReleaseStrong());
 } }
 public static int Main() { unsafe("shared block") {
  if (SharedControlStorage.AllocateLayout(-1l, 8l) != null ||
      SharedControlStorage.AllocateLayout(0l, 8l) != null ||
      SharedControlStorage.AllocateLayout(8l, 0l) != null ||
      SharedControlStorage.AllocateLayout(8l, 3l) != null ||
      SharedControlStorage.AllocateLayout(8l, 2147483648l) != null ||
      SharedControlStorage.AllocateLayout(17179869176l, 8l) != null ||
      SharedControlStorage.AllocateLayout(9223372036854775807l, 8l) != null) return 13;
  long[] tiny = SharedControlStorage.AllocateLayout(1l, 1l);
  if (tiny == null || tiny.Length != 9) return 14; delete tiny;
  if (SharedControlStorage.Construct<Payload>(null) != null) return 8;
  long[] shortBlock = new long[8];
  if (SharedControlStorage.Construct<Payload>(shortBlock) != null) return 9;
  delete shortBlock;
  long[] block = SharedControlStorage.Allocate<Payload>(); if (block == null) return 10;
  long* baseAddress = &block[0]; COUNTS* counts = (COUNTS*)baseAddress;
  *counts = new COUNTS(1l, 1l); THREAD_TEST
  Payload item = SharedControlStorage.Construct<Payload>(block); if (item == null) return 11;
  if (SharedStorage.InstanceAlignment<Payload>(null) != 32l || (long)&item.A % 32l != 0l) return 12;
  item.A = 40l; item.B = 2l;
  if (!(*counts).TryRetainStrong() || !(*counts).TryRetainWeak()) return 1;
  if ((*counts).StrongCount() != 2l || (*counts).WeakCountRaw() != 2l || item.A + item.B != 42l) return 2;
  if ((*counts).ReleaseStrong() != 0 || (*counts).ReleaseStrong() != 1) return 3;
  SharedStorage.DestroyObject<Payload>(item); if (Payload.Drops != 1 || (*counts).TryRetainStrong()) return 4;
  if ((*counts).ReleaseWeak() != 0 || (*counts).ReleaseWeak() != 1) return 5;
  delete block; return 42;
 } }
}
'''
    thread_test = '''globalCounts = counts;
  ThreadStart start = Work; raii Thread a = new Thread(start); raii Thread b = new Thread(start); raii Thread c = new Thread(start);
  Work(); a.Join(); b.Join(); c.Join();
  if (SharedAtomic.Load(ref failures) != 0l || (*counts).StrongCount() != 1l) return 6;
  int round = 0;
  while (round < 1000) {
   *counts = new AtomicSharedCounts(1l, 1l); ready = 0l; last = 0l;
   ThreadStart raceStart = RaceWork; raii Thread race = new Thread(raceStart);
   while (SharedAtomic.Load(ref ready) == 0l) { }
   RecordRelease((*counts).ReleaseStrong()); race.Join();
   if (SharedAtomic.Load(ref failures) != 0l || SharedAtomic.Load(ref last) != 1l ||
       (*counts).StrongCount() != 0l || (*counts).TryRetainStrong()) return 7;
   round = round + 1;
  }
  *counts = new AtomicSharedCounts(1l, 1l);'''
    with tempfile.TemporaryDirectory(prefix='nsc-shared-control-') as temporary:
        root = Path(temporary)
        for path in (storage_source, control_source, counts_source, thread_source):
            shutil.copyfile(path, root / path.name)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        for counts, threaded in (('LocalSharedCounts', False), ('AtomicSharedCounts', True)):
            program = source.replace('COUNTS', counts).replace('THREAD_TEST', thread_test if threaded else '')
            (root / 'Main.ns').write_text(program, encoding='utf-8')
            result = run([compiler, '--check-project-oop-hir', project], root)
            assert result.returncode == 0, result.stdout + result.stderr
            if os.name == 'nt':
                for level in ('O0', 'O2'):
                    exe = root / (counts + level + '.exe')
                    result = run([compiler, '--emit-project-oop-exe', project, '-o', exe,
                                  '--no-artifact-cache', '-' + level], root)
                    assert result.returncode == 0, result.stdout + result.stderr
                    result = run([exe], root)
                    assert result.returncode == 42, (counts, level, result.returncode, result.stdout, result.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
