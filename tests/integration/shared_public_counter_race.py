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
    compiler, repo = map(Path, sys.argv[1:])
    names = ('SharedStorage', 'SharedControlStorage', 'SharedReferenceCounts', 'SharedLocalControl',
             'SharedAtomicControl', 'SharedBorrow', 'SharedLocalHandle', 'SharedLocalWeak',
             'SharedLocalFactory', 'SharedAtomicHandle', 'SharedAtomicWeak', 'SharedAtomicFactory',
             'SharedView', 'Rc', 'WeakRc', 'Arc', 'WeakArc', 'SharedPointers')
    program = '''using NativeSharp.Core; using NativeSharp.Core.Internal; using NativeSharp.Core.Threading;
namespace NativeSharp.Core { public class Exception { } }
class Payload { public static int Drops; public int Value; public Payload() { Value = 42; }
 ~Payload() { Drops = Drops + 1; } }
class Program {
 private static WeakArc<Payload> weak; private static long gate;
 private static long readyA; private static long readyB; private static long resultA; private static long resultB;
 public static void WorkerA() {
  while (SharedAtomic.Load(ref gate) == 0l) { }
  { Arc<Payload> owner = weak.Upgrade();
    SharedAtomic.CompareExchange(ref resultA, owner.IsEmpty() ? 1l : 2l, 0l);
    SharedAtomic.CompareExchange(ref readyA, 1l, 0l);
    while (SharedAtomic.Load(ref gate) != 2l) { }
  }
 }
 public static void WorkerB() {
  while (SharedAtomic.Load(ref gate) == 0l) { }
  { Arc<Payload> owner = weak.Upgrade();
    SharedAtomic.CompareExchange(ref resultB, owner.IsEmpty() ? 1l : 2l, 0l);
    SharedAtomic.CompareExchange(ref readyB, 1l, 0l);
    while (SharedAtomic.Load(ref gate) != 2l) { }
  }
 }
 public static int Main() { int failures = 0; int round = 0;
  while (round < 32) { gate = 0l; readyA = 0l; readyB = 0l; resultA = 0l; resultB = 0l;
   unsafe("single-threaded setup and synchronized counter boundary") {
    long[] block = SharedControlStorage.Allocate<Payload>();
    Payload payload = SharedControlStorage.Construct<Payload>(block);
    Payload* slot = (Payload*)&block[6]; *slot = payload; SharedAtomicControl.Initialize(block);
    { SharedAtomicHandle<Payload> initial = new SharedAtomicHandle<Payload>(block, slot);
      Arc<Payload> owner = new Arc<Payload>(move(initial)); weak = owner.Downgrade();
      block[0] = 9223372036854775806l;
      ThreadStart startA = WorkerA; ThreadStart startB = WorkerB;
      raii Thread a = new Thread(startA); raii Thread b = new Thread(startB);
      SharedAtomic.CompareExchange(ref gate, 1l, 0l);
      while (SharedAtomic.Load(ref readyA) == 0l || SharedAtomic.Load(ref readyB) == 0l) { }
      if (SharedAtomic.Load(ref resultA) + SharedAtomic.Load(ref resultB) != 3l ||
          SharedAtomic.Load(ref block[0]) != 9223372036854775807l) failures = failures + 1;
      SharedAtomic.CompareExchange(ref gate, 2l, 1l); a.Join(); b.Join();
      if (SharedAtomic.Load(ref block[0]) != 9223372036854775806l) failures = failures + 1;
      block[0] = 1l;
      if (owner.Get().Value != 42 || Payload.Drops != round) failures = failures + 1;
    }
    if (Payload.Drops != round + 1 || !weak.Expired()) failures = failures + 1;
    weak = default(WeakArc<Payload>);
   }
   round = round + 1;
  }
  return failures == 0 ? 42 : failures;
 }
}'''
    with tempfile.TemporaryDirectory(prefix='nsc-public-counter-race-') as temporary:
        root = Path(temporary)
        for name in names:
            shutil.copyfile(repo / 'lib' / 'NativeSharp.Core' / (name + '.ns'), root / (name + '.ns'))
        shutil.copyfile(repo / 'lib' / 'NativeSharp.Core.Threading' / 'Thread.ns', root / 'Thread.ns')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        assert program.count('Arc<Payload> owner = weak.Upgrade();') == 2
        assert program.count('owner.IsEmpty()') == 2
        assert program.count('block[0] = 1l;') == 1
        for counter in ('strong', 'weak'):
            source = program
            if counter == 'weak':
                source = (source.replace('Arc<Payload> owner = weak.Upgrade();',
                                         'WeakArc<Payload> owner = weak.Clone();')
                          .replace('owner.IsEmpty()', 'owner.Expired()')
                          .replace('block[0] = 1l;', 'block[0] = 2l;')
                          .replace('block[0]', 'block[1]'))
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / (counter + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-o', exe, '-' + level], root)
                assert result.returncode == 0, (counter, level, result.stdout + result.stderr)
                result = run([exe], root)
                assert result.returncode == 42, (counter, level, result.returncode, result.stdout, result.stderr)
                print(counter, level, 'exit=42', flush=True)
    return 0


if __name__ == '__main__':
    if os.name != 'nt':
        raise SystemExit('this directed test requires native Windows execution')
    sys.exit(main())
