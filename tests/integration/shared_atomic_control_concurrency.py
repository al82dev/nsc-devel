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
 private static long finals; private static long failures;
 public static void Work() { int result = SharedAtomicControl.DropStrong<Payload>(block, slot);
  if (result == 1) { if (SharedAtomic.CompareExchange(ref finals, 1l, 0l) != 0l)
   SharedAtomic.CompareExchange(ref failures, 1l, 0l); }
  else if (result != 0) SharedAtomic.CompareExchange(ref failures, 1l, 0l);
 }
 public static int Main() { unsafe("shared atomic control test") {
  int round = 0; while (round < 64) {
   block = SharedControlStorage.Allocate<Payload>(); Payload value = SharedControlStorage.Construct<Payload>(block);
   slot = (Payload*)&block[6]; *slot = value; finals = 0l; failures = 0l;
   SharedAtomicControl.Initialize(block);
   if (!SharedAtomicControl.TryRetainWeak(block) || !SharedAtomicControl.TryRetainStrong(block) ||
       !SharedAtomicControl.TryRetainStrong(block) || !SharedAtomicControl.TryRetainStrong(block)) return 1;
   ThreadStart start = Work; raii Thread a = new Thread(start); raii Thread b = new Thread(start);
   raii Thread c = new Thread(start); Work(); a.Join(); b.Join(); c.Join();
   if (SharedAtomic.Load(ref failures) != 0l || SharedAtomic.Load(ref finals) != 1l ||
       Payload.Drops != round + 1 || SharedAtomicControl.StrongCount(block) != 0l ||
       SharedAtomicControl.TryRetainStrong(block)) return 2;
   if (SharedAtomicControl.DropWeak(block) != 1) return 3; round = round + 1;
  } return 42;
 } }
}'''
    with tempfile.TemporaryDirectory(prefix='nsc-shared-atomic-control-') as temporary:
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
                result = run([compiler, '--emit-project-oop-exe', project, '-o', exe,
                              '--no-artifact-cache', '-' + level], root)
                assert result.returncode == 0, (level, result.stdout + result.stderr)
                result = run([exe], root)
                assert result.returncode == 42, (level, result.returncode, result.stdout, result.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
