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
class Payload {
 public static long Ready; public static long Continue; public static int Drops;
 ~Payload() { SharedAtomic.CompareExchange(ref Ready, 1l, 0l);
  while (SharedAtomic.Load(ref Continue) == 0l) { } Drops = Drops + 1; }
}
class Program {
 private static long[] block; private static Payload* slot; private static long result;
 public static void Release() {
  int observed = SharedAtomicControl.DropStrong<Payload>(block, slot);
  SharedAtomic.CompareExchange(ref result, (long)observed, 0l);
 }
 public static int Main() { unsafe("last explicit weak during destruction") {
  int round = 0; while (round < 64) {
   block = SharedControlStorage.Allocate<Payload>();
   Payload value = SharedControlStorage.Construct<Payload>(block);
   slot = (Payload*)&block[6]; *slot = value; result = 0l;
   Payload.Ready = 0l; Payload.Continue = 0l;
   SharedAtomicControl.Initialize(block);
   if (!SharedAtomicControl.TryRetainWeak(block)) return 1;
   ThreadStart start = Release; raii Thread worker = new Thread(start);
   while (SharedAtomic.Load(ref Payload.Ready) == 0l) { }
   int weakResult = SharedAtomicControl.DropWeak(block);
   SharedAtomic.CompareExchange(ref Payload.Continue, 1l, 0l);
   worker.Join();
   if (weakResult != 0 || SharedAtomic.Load(ref result) != 1l || Payload.Drops != round + 1) return 2;
   round = round + 1;
  } return 42;
 } }
}'''
    with tempfile.TemporaryDirectory(prefix='nsc-shared-last-weak-') as temporary:
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
                print(level, '64 interleavings exit=42', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
