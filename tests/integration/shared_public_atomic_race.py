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
    compiler = Path(sys.argv[1])
    repo = Path(sys.argv[2])
    program = '''using NativeSharp.Core; using NativeSharp.Core.Internal; using NativeSharp.Core.Threading;
namespace NativeSharp.Core { public class Exception { } }
class Payload { public static int Drops; public int Value; public Payload() { Value = 42; }
 ~Payload() { Drops = Drops + 1; } }
class Program {
 private static WeakArc<Payload> weak; private static long gate; private static long ready; private static long failures;
 public static void Worker() {
  while (SharedAtomic.Load(ref gate) == 0l) { }
  { Arc<Payload> first = weak.Upgrade();
    if (first.IsEmpty() || first.Get().Value != 42) SharedAtomic.CompareExchange(ref failures, 1l, 0l);
  }
  SharedAtomic.CompareExchange(ref ready, 1l, 0l);
  while (SharedAtomic.Load(ref gate) != 2l) { }
  bool expired = false; int index = 0;
  while (index < 64) {
   { Arc<Payload> owner = weak.Upgrade();
     if (owner.IsEmpty()) expired = true;
     else {
      if (expired || owner.Get().Value != 42) SharedAtomic.CompareExchange(ref failures, 1l, 0l);
     }
   }
   index = index + 1;
  }
 }
 public static int Main() {
  int round = 0;
  while (round < 128) {
   gate = 0l; ready = 0l; failures = 0l;
   ThreadStart start = Worker; raii Thread worker = new Thread(start);
   { Arc<Payload> owner = SharedPointers.MakeArc<Payload>();
     weak = owner.Downgrade(); SharedAtomic.CompareExchange(ref gate, 1l, 0l);
     while (SharedAtomic.Load(ref ready) == 0l) { }
     SharedAtomic.CompareExchange(ref gate, 2l, 1l);
   }
   worker.Join();
   if (SharedAtomic.Load(ref failures) != 0l || !weak.Expired() || weak.WeakCount() != 1l ||
       Payload.Drops != round + 1) return 1;
   { Arc<Payload> dead = weak.Upgrade(); if (!dead.IsEmpty()) return 2; }
   round = round + 1;
  }
  return 42;
 }
}'''
    if os.environ.get('NSC_SHARED_RACE_LAST_STRONG') == '1':
        program = '''using NativeSharp.Core; using NativeSharp.Core.Internal; using NativeSharp.Core.Threading;
namespace NativeSharp.Core { public class Exception { } }
class Payload { public static int Drops; ~Payload() { Drops = Drops + 1; } }
class Program {
 private static WeakArc<Payload> weak; private static long gate; private static long ready; private static long failures;
 public static void Worker() {
  while (SharedAtomic.Load(ref gate) == 0l) { }
  { WeakArc<Payload> held = weak.Clone(); SharedAtomic.CompareExchange(ref ready, 1l, 0l);
    while (SharedAtomic.Load(ref gate) != 2l) { }
    { Arc<Payload> dead = held.Upgrade();
      if (!held.Expired() || !dead.IsEmpty()) SharedAtomic.CompareExchange(ref failures, 1l, 0l); }
  }
 }
 public static int Main() {
  int round = 0;
  while (round < 128) {
   gate = 0l; ready = 0l; failures = 0l;
   ThreadStart start = Worker; raii Thread worker = new Thread(start);
   { Arc<Payload> owner = SharedPointers.MakeArc<Payload>(); weak = owner.Downgrade();
     SharedAtomic.CompareExchange(ref gate, 1l, 0l); while (SharedAtomic.Load(ref ready) == 0l) { }
     if (weak.WeakCount() != 2l || Payload.Drops != round) SharedAtomic.CompareExchange(ref failures, 1l, 0l);
   }
   if (Payload.Drops != round + 1 || !weak.Expired()) SharedAtomic.CompareExchange(ref failures, 1l, 0l);
   SharedAtomic.CompareExchange(ref gate, 2l, 1l); worker.Join();
   if (SharedAtomic.Load(ref failures) != 0l || weak.WeakCount() != 1l) return 3;
   round = round + 1;
  }
  return 42;
 }
}'''
    if os.environ.get('NSC_SHARED_RACE_WEAK_CLONE') == '1':
        assert '{ Arc<Payload> owner = weak.Upgrade();' in program
        assert 'SharedAtomic.CompareExchange(ref gate, 2l, 1l);' in program
        program = program.replace(
            '{ Arc<Payload> owner = weak.Upgrade();',
            '{ WeakArc<Payload> transient = weak.Clone(); Arc<Payload> owner = transient.Upgrade();')
        program = program.replace(
            'SharedAtomic.CompareExchange(ref gate, 2l, 1l);',
            'SharedAtomic.CompareExchange(ref gate, 2l, 1l); int clones = 0; '
            'while (clones < 64) { WeakArc<Payload> transient = weak.Clone(); clones = clones + 1; }')
    local_negative = os.environ.get('NSC_SHARED_RACE_LOCAL_NEGATIVE') == '1'
    if local_negative:
        program = program.replace('WeakArc<', 'WeakRc<').replace('Arc<', 'Rc<')
        program = program.replace('MakeArc<', 'MakeRc<')
    names = ('SharedStorage', 'SharedControlStorage', 'SharedReferenceCounts', 'SharedLocalControl',
             'SharedAtomicControl', 'SharedBorrow', 'SharedLocalHandle', 'SharedLocalWeak',
             'SharedLocalFactory', 'SharedAtomicHandle', 'SharedAtomicWeak', 'SharedAtomicFactory',
             'SharedView', 'Rc', 'WeakRc', 'Arc', 'WeakArc', 'SharedPointers')
    with tempfile.TemporaryDirectory(prefix='nsc-public-atomic-race-') as temporary:
        root = Path(temporary)
        for name in names:
            shutil.copyfile(repo / 'lib' / 'NativeSharp.Core' / (name + '.ns'), root / (name + '.ns'))
        shutil.copyfile(repo / 'lib' / 'NativeSharp.Core.Threading' / 'Thread.ns', root / 'Thread.ns')
        (root / 'Main.ns').write_text(program, encoding='utf-8')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        check = run([compiler, '--check-project-method-bodies', project], root)
        if local_negative:
            assert check.returncode != 0 and 'thread-local shared pointer' in (check.stdout + check.stderr), \
                (check.returncode, check.stdout + check.stderr)
            print('local family rejected:', check.stdout + check.stderr, flush=True)
            return 0
        assert check.returncode == 0, check.stdout + check.stderr
        if os.name == 'nt':
            for level in ('O0', 'O2'):
                exe = root / (level + '.exe')
                build = run([compiler, '--emit-project-oop-exe', project, '-o', exe, '-' + level], root)
                assert build.returncode == 0, (level, build.stdout + build.stderr)
                result = run([exe], root)
                assert result.returncode == 42, (level, result.returncode, result.stdout, result.stderr)
                print(level, 'exit=42', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
