"""Exercise binary WeakArc promotion against the last strong release."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


SOURCE = '''using NativeSharp.Core; using NativeSharp.Core.Threading;
class Payload { public static int Drops; public int Value;
 public Payload() { Value = 42; } ~Payload() { Drops = Drops + 1; } }
class Program {
 private static WeakArc<Payload> weak; private static int gate; private static int ready;
 private static int failures;
 public static void Worker() {
  while (Interlocked.Load(ref gate) == 0) { }
  { Arc<Payload> first = weak.Upgrade();
    if (first.IsEmpty() || first.Get().Value != 42) failures = 1; }
  Interlocked.Store(ref ready, 1);
  while (Interlocked.Load(ref gate) != 2) { }
  bool expired = false; int index = 0;
  while (index < 128) {
   { Arc<Payload> current = weak.Upgrade();
     if (current.IsEmpty()) expired = true;
     else if (expired || current.Get().Value != 42) failures = 1; }
   index = index + 1;
  }
 }
 public static int Main() { int round = 0;
  while (round < 128) { gate = 0; ready = 0; failures = 0;
   ThreadStart start = Worker; raii Thread worker = new Thread(start);
   { Arc<Payload> owner = SharedPointers.MakeArc<Payload>();
     weak = owner.Downgrade(); Interlocked.Store(ref gate, 1);
     while (Interlocked.Load(ref ready) == 0) { }
     Interlocked.Store(ref gate, 2);
   }
   worker.Join();
   if (failures != 0 || !weak.Expired() || weak.WeakCount() != 1l ||
       Payload.Drops != round + 1) return 1;
   { Arc<Payload> dead = weak.Upgrade(); if (!dead.IsEmpty()) return 2; }
   round = round + 1;
  }
  return 42;
 }
}'''


def main():
    if os.name != 'nt' or len(sys.argv) != 3:
        return 77
    compiler, core = (Path(value).resolve() for value in sys.argv[1:])
    temp = Path(os.environ.get('TEMP', '')).resolve()
    if any(path.drive.upper() != 'A:' for path in (compiler, core, temp)):
        raise SystemExit('compiler, Core and TEMP must be on A:')
    if not compiler.is_file() or not core.is_file() or not core.with_suffix('.nsmeta').is_file():
        raise SystemExit('compiler and Core with metadata are required')
    repository = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix='nsc-arc-binary-race-', dir=temp) as temporary:
        root = Path(temporary)
        for name in ('Thread.ns', 'Atomics.ns'):
            shutil.copyfile(repository / 'lib/NativeSharp.Core.Threading' / name, root / name)
        (root / 'Main.ns').write_text(SOURCE, encoding='utf-8')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>ArcBinaryRace</Name></PropertyGroup>'
            '<ItemGroup><Compile Include="*.ns"/><LibraryReference Include="' + core.as_posix() +
            '" Metadata="' + core.with_suffix('.nsmeta').as_posix() + '"/></ItemGroup></Project>',
            encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            print('CWD', repository, 'COMMAND', subprocess.list2cmdline(command), flush=True)
            result = subprocess.run(command, cwd=repository, capture_output=True, text=True,
                                    errors='replace', timeout=120)
            if result.returncode:
                print('FAIL compile', level, result.stdout + result.stderr, file=sys.stderr)
                return 1
            result = subprocess.run([str(executable)], cwd=repository, capture_output=True,
                                    text=True, errors='replace', timeout=20)
            print('RUN', level, result.returncode, flush=True)
            if result.returncode != 42:
                print('FAIL run', level, result.stdout + result.stderr, file=sys.stderr)
                return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
