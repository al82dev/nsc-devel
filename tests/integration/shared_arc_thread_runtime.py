"""Execute isolated Arc clone/upgrade/drop work on two native threads."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(command, cwd):
    command = [str(value) for value in command]
    print('CWD', cwd, 'COMMAND', subprocess.list2cmdline(command), flush=True)
    return subprocess.run(command, cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler = Path(sys.argv[1]).resolve()
    library = Path(sys.argv[2]).resolve()
    repo = Path(__file__).resolve().parents[2]
    source = '''using NativeSharp.Core; using NativeSharp.Core.Threading;
class Payload { public static int Drops; public int Value;
 public Payload() { Value = 42; } ~Payload() { Drops = Drops + 1; } }
class Worker {
 public static int Ready; public static int Gate;
 private Arc<Payload> owner;
 public int Result;
 public Worker(Arc<Payload> value) { owner = move(value); Result = 0; }
 public void Run() {
  Interlocked.Increment(ref Ready); while (Interlocked.Load(ref Gate) == 0) { }
  int i = 0; while (i < 1000) {
   Arc<Payload> local = owner.Clone(); WeakArc<Payload> weak = local.Downgrade();
   Arc<Payload> upgraded = weak.Upgrade();
   if (local.IsEmpty() || upgraded.IsEmpty() || upgraded.Get().Value != 42) { Result = 1; }
   i = i + 1;
  }
 }
 public void RunAndRelease() { Run(); owner = default(Arc<Payload>); }
}
class Program {
 static int RequireSend<T>() where T : Send { return 1; }
 static int RequireSync<T>() where T : Sync { return 1; }
 public static int Main() {
 if (RequireSend<Arc<Payload>>() != 1 || RequireSync<Arc<Payload>>() != 1 ||
     RequireSend<WeakArc<Payload>>() != 1 || RequireSync<WeakArc<Payload>>() != 1) return 8;
 { Arc<Payload> owner = SharedPointers.MakeArc<Payload>();
  { Arc<Payload> copyA = owner.Clone(); Arc<Payload> copyB = owner.Clone();
   raii Worker first = new Worker(move(copyA)); raii Worker second = new Worker(move(copyB));
   ThreadStart startA = first.Run; ThreadStart startB = second.Run;
   raii Thread a = new Thread(startA); raii Thread b = new Thread(startB);
   while (Interlocked.Load(ref Worker.Ready) != 2) { }
   Interlocked.Store(ref Worker.Gate, 1);
   a.Join(); b.Join();
   if (first.Result != 0 || second.Result != 0) return 1;
   if (owner.StrongCount() != 3l || owner.WeakCount() != 0l) return 2;
  }
  if (owner.StrongCount() != 1l || Payload.Drops != 0) return 3;
 }
 if (Payload.Drops != 1) return 4;
 Worker.Ready = 0; Worker.Gate = 0;
 { Arc<Payload> owner = SharedPointers.MakeArc<Payload>();
  WeakArc<Payload> observer = owner.Downgrade();
  { Arc<Payload> copyA = owner.Clone(); Arc<Payload> copyB = owner.Clone();
   raii Worker first = new Worker(move(copyA)); raii Worker second = new Worker(move(copyB));
   { Arc<Payload> released = move(owner); }
   ThreadStart startA = first.RunAndRelease; ThreadStart startB = second.RunAndRelease;
   raii Thread a = new Thread(startA); raii Thread b = new Thread(startB);
   while (Interlocked.Load(ref Worker.Ready) != 2) { }
   Interlocked.Store(ref Worker.Gate, 1); a.Join(); b.Join();
   if (first.Result != 0 || second.Result != 0) return 5;
   if (!observer.Expired() || observer.WeakCount() != 1l || Payload.Drops != 2) return 6;
  }
 }
 return Payload.Drops == 2 ? 42 : 7;
} }
'''
    with tempfile.TemporaryDirectory(prefix='nsc-arc-thread-runtime-') as temporary:
        root = Path(temporary)
        shutil.copyfile(repo / 'lib/NativeSharp.Core.Threading/Thread.ns', root / 'Thread.ns')
        shutil.copyfile(repo / 'lib/NativeSharp.Core.Threading/Atomics.ns', root / 'Atomics.ns')
        (root / 'Main.ns').write_text(source, encoding='utf-8')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>ArcThreadRuntime</Name>'
                           '</PropertyGroup><ItemGroup><Compile Include="*.ns" />'
                           '<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                           library.with_suffix('.nsmeta').as_posix() + '" />'
                           '</ItemGroup></Project>', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / ('runtime-' + level + '.exe')
            result = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', executable,
                          '--no-artifact-cache'], repo)
            print(level, 'compile=' + str(result.returncode), flush=True)
            if result.returncode != 0:
                print(result.stdout + result.stderr, file=sys.stderr)
                return 1
            result = run([executable], repo)
            print(level, 'run=' + str(result.returncode), flush=True)
            if result.returncode != 42:
                print(result.stdout + result.stderr, file=sys.stderr)
                return 1
    return 0


if __name__ == '__main__':
    if os.name != 'nt':
        raise SystemExit('this directed test requires native Windows execution')
    sys.exit(main())
