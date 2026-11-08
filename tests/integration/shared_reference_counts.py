import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    compiler, counts_source, thread_source = map(Path, sys.argv[1:4])
    with tempfile.TemporaryDirectory(prefix='nsc-shared-counts-') as tmp:
        root = Path(tmp)
        shutil.copyfile(counts_source, root / 'SharedReferenceCounts.ns')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        (root / 'Main.ns').write_text('''using NativeSharp.Core.Internal;
class Program {
 public static int Main() {
  AtomicSharedCounts count = new AtomicSharedCounts(1l, 1l);
  if (!count.TryRetainStrong() || !count.TryRetainWeak()) return 1;
  if (count.StrongCount() != 2l || count.WeakCountRaw() != 2l) return 2;
  if (count.ReleaseStrong() != 0 || count.ReleaseStrong() != 1) return 3;
  if (count.TryRetainStrong() || count.ReleaseStrong() != -1) return 4;
  if (count.ReleaseWeak() != 0 || count.ReleaseWeak() != 1) return 5;
  if (count.TryRetainWeak() || count.ReleaseWeak() != -1) return 6;
  AtomicSharedCounts full = new AtomicSharedCounts(9223372036854775807l, 9223372036854775807l);
  if (full.TryRetainStrong() || full.TryRetainWeak()) return 7;
  if (full.StrongCount() != 9223372036854775807l || full.WeakCountRaw() != 9223372036854775807l) return 8;
  return 42;
 }
}''', encoding='utf-8')
        result = subprocess.run([str(compiler), '--dump-project-oop-llvm', str(project), '--no-banner'],
                                cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'call i64 @__nativesharp_atomic_load_int64' in result.stdout, 'atomic load missing from LLVM IR'
        assert 'call i64 @__nativesharp_interlocked_compare_exchange_int64' in result.stdout, 'atomic compare-exchange missing from LLVM IR'
        if os.name == 'nt':
            for level in ('O0', 'O2'):
                exe = root / (level + '.exe')
                result = subprocess.run([str(compiler), '--emit-project-oop-exe', str(project), '-o', str(exe),
                                         '--no-artifact-cache', '-' + level], cwd=root, capture_output=True,
                                        text=True, errors='replace', timeout=120)
                assert result.returncode == 0, result.stdout + result.stderr
                result = subprocess.run([str(exe)], cwd=root, capture_output=True, timeout=60)
                assert result.returncode == 42, result.returncode
            shutil.copyfile(thread_source, root / 'Thread.ns')
            (root / 'Main.ns').write_text('''using NativeSharp.Core.Internal;
using NativeSharp.Core.Threading;
class Program {
 private static AtomicSharedCounts counts;
 public static void Work() {
  int i = 0;
  while (i < 50000) { if (counts.TryRetainStrong()) counts.ReleaseStrong(); i = i + 1; }
 }
 public static int Main() {
  counts = new AtomicSharedCounts(1l, 1l);
  ThreadStart start = Work;
  raii Thread a = new Thread(start);
  raii Thread b = new Thread(start);
  raii Thread c = new Thread(start);
  Work(); a.Join(); b.Join(); c.Join();
  return counts.StrongCount() == 1l ? 42 : 1;
 }
}''', encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / ('stress-' + level + '.exe')
                result = subprocess.run([str(compiler), '--emit-project-oop-exe', str(project), '-o', str(exe),
                                         '--no-artifact-cache', '-' + level], cwd=root, capture_output=True,
                                        text=True, errors='replace', timeout=120)
                assert result.returncode == 0, result.stdout + result.stderr
                result = subprocess.run([str(exe)], cwd=root, capture_output=True, timeout=60)
                assert result.returncode == 42, result.returncode
    return 0


if __name__ == '__main__':
    sys.exit(main())
