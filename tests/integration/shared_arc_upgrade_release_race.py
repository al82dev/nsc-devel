"""Exercise a WeakArc upgrade while another thread drops the last strong Arc."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(command, cwd):
    result = subprocess.run([str(part) for part in command], cwd=cwd, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', *command, '\nEXIT', result.returncode, flush=True)
    if result.stdout or result.stderr:
        print(result.stdout + result.stderr, flush=True)
    return result.returncode


def main():
    if os.name != 'nt':
        raise SystemExit('this directed test requires native Windows execution')
    compiler, core = (Path(value).resolve() for value in sys.argv[1:3])
    repo = Path(__file__).resolve().parents[2]
    source = '''using NativeSharp.Core; using NativeSharp.Core.Threading;
class Payload { public static int Drops; public int Value;
 public Payload() { Value = 42; } ~Payload() { Drops = Drops + 1; } }
class Worker {
 public static int Ready; public static int Gate;
 private Arc<Payload> strong; private WeakArc<Payload> weak; public int Result;
 public Worker(Arc<Payload> value, WeakArc<Payload> observer) {
  strong = move(value); weak = move(observer); Result = 0;
 }
 public void Drop() {
  Interlocked.Increment(ref Ready); while (Interlocked.Load(ref Gate) == 0) { }
  strong = default(Arc<Payload>);
 }
 public void Upgrade() {
  Interlocked.Increment(ref Ready); while (Interlocked.Load(ref Gate) == 0) { }
  int i = 0; while (i < 10000) {
   Arc<Payload> owner = weak.Upgrade();
   if (!owner.IsEmpty() && owner.Get().Value != 42) Result = 1;
   i = i + 1;
  }
 }
}
class ControlledWorker {
 public static int Promoted; public static int OriginalReleased;
 private Arc<Payload> strong; private WeakArc<Payload> weak; public int Result;
 public ControlledWorker(Arc<Payload> value, WeakArc<Payload> observer) {
  strong = move(value); weak = move(observer); Result = 0;
 }
 public void DropOriginal() {
  while (Interlocked.Load(ref Promoted) == 0) { }
  strong = default(Arc<Payload>);
  Interlocked.Store(ref OriginalReleased, 1);
 }
 public void PromoteAndHold() {
  Arc<Payload> retained = weak.Upgrade();
  if (retained.IsEmpty()) Result = 1;
  else if (retained.Get().Value != 42) Result = 2;
  Interlocked.Store(ref Promoted, 1);
  while (Interlocked.Load(ref OriginalReleased) == 0) { }
  if (retained.IsEmpty() || weak.Expired()) Result = 3;
  else if (retained.Get().Value != 42) Result = 4;
  retained = default(Arc<Payload>);
 }
}
class Program { public static int Main() {
 WeakArc<Payload> observer;
 { Arc<Payload> owner = SharedPointers.MakeArc<Payload>();
   observer = owner.Downgrade(); WeakArc<Payload> other = observer.Clone();
   raii Worker dropper = new Worker(move(owner), default(WeakArc<Payload>));
   raii Worker reader = new Worker(default(Arc<Payload>), move(other));
   ThreadStart drop = dropper.Drop; ThreadStart upgrade = reader.Upgrade;
   raii Thread first = new Thread(drop); raii Thread second = new Thread(upgrade);
   while (Interlocked.Load(ref Worker.Ready) != 2) { }
   Interlocked.Store(ref Worker.Gate, 1); first.Join(); second.Join();
   if (reader.Result != 0 || !observer.Expired() || Payload.Drops != 1) return 1;
   Arc<Payload> after = observer.Upgrade(); if (!after.IsEmpty()) return 2;
 }
 if (!observer.Expired() || Payload.Drops != 1) return 3;
 WeakArc<Payload> controlledObserver;
 { Arc<Payload> owner = SharedPointers.MakeArc<Payload>();
   controlledObserver = owner.Downgrade(); WeakArc<Payload> other = controlledObserver.Clone();
   raii ControlledWorker dropper = new ControlledWorker(move(owner), default(WeakArc<Payload>));
   raii ControlledWorker reader = new ControlledWorker(default(Arc<Payload>), move(other));
   ThreadStart drop = dropper.DropOriginal; ThreadStart upgrade = reader.PromoteAndHold;
   raii Thread first = new Thread(drop); raii Thread second = new Thread(upgrade);
   first.Join(); second.Join();
   if (reader.Result != 0 || !controlledObserver.Expired() || Payload.Drops != 2) return 4;
   Arc<Payload> after = controlledObserver.Upgrade(); if (!after.IsEmpty()) return 5;
 }
 return controlledObserver.Expired() && Payload.Drops == 2 ? 42 : 6;
} }
'''
    with tempfile.TemporaryDirectory(prefix='nsc-arc-upgrade-race-') as temporary:
        root = Path(temporary)
        for name in ('Thread.ns', 'Atomics.ns'):
            shutil.copyfile(repo / 'lib/NativeSharp.Core.Threading' / name, root / name)
        (root / 'Main.ns').write_text(source, encoding='utf-8')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>ArcUpgradeRace</Name>'
                           '</PropertyGroup><ItemGroup><Compile Include="*.ns" />'
                           '<LibraryReference Include="' + core.as_posix() + '" Metadata="' +
                           core.with_suffix('.nsmeta').as_posix() + '" />'
                           '</ItemGroup></Project>', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / ('race-' + level + '.exe')
            if run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', executable,
                    '--no-artifact-cache'], root) != 0:
                return 1
            if run([executable], root) != 42:
                return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
