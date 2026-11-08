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
    program = '''using NativeSharp.Core.Internal;
namespace NativeSharp.Core { public class Exception { } }
class Payload { public static int Drops; public long Value; ~Payload() { Drops = Drops + 1; } }
struct Value { public static int Drops; public long Data; ~Value() { Drops = Drops + 1; } }
class Program {
 public static int Main() { unsafe("shared local control test") {
  long[] classes = SharedControlStorage.Allocate<Payload>();
  Payload value = SharedControlStorage.Construct<Payload>(classes);
  Payload* classSlot = (Payload*)&classes[6]; *classSlot = value; value.Value = 42l;
  SharedLocalControl.Initialize(classes);
  if (!SharedLocalControl.TryRetainStrong(classes) || !SharedLocalControl.TryRetainWeak(classes)) return 1;
  if (SharedLocalControl.StrongCount(classes) != 2l || value.Value != 42l) return 2;
  if (SharedLocalControl.DropStrong<Payload>(classes, classSlot) != 0 || Payload.Drops != 0) return 3;
  if (SharedLocalControl.DropStrong<Payload>(classes, classSlot) != 1 || Payload.Drops != 1) return 4;
  if (SharedLocalControl.StrongCount(classes) != 0l || SharedLocalControl.TryRetainStrong(classes)) return 5;
  if (SharedLocalControl.DropStrong<Payload>(classes, classSlot) != -1 || Payload.Drops != 1) return 12;
  if (SharedLocalControl.DropWeak(classes) != 1) return 6;
  long[] values = SharedControlStorage.AllocateValue<Value>();
  Value* item = SharedControlStorage.ConstructValue<Value>(values); (*item).Data = 42l;
  SharedLocalControl.Initialize(values);
  if (!SharedLocalControl.TryRetainWeak(values)) return 7;
  if (SharedLocalControl.DropStrong<Value>(values, item) != 1 || Value.Drops != 1) return 8;
  if (SharedLocalControl.TryRetainStrong(values) || SharedLocalControl.DropWeak(values) != 1) return 9;
  long[] limits = SharedControlStorage.AllocateValue<Value>();
  COUNTS* counts = (COUNTS*)&limits[0];
  *counts = new COUNTS(9223372036854775807l, 1l);
  if (SharedLocalControl.TryRetainStrong(limits)) return 10;
  *counts = new COUNTS(1l, 9223372036854775807l);
  if (SharedLocalControl.TryRetainWeak(limits)) return 11;
  delete[] limits;
  return 42;
 } }
}'''
    with tempfile.TemporaryDirectory(prefix='nsc-shared-local-control-') as temporary:
        root = Path(temporary)
        for source in sources:
            shutil.copyfile(source, root / source.name)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        for control, counts in (('SharedLocalControl', 'LocalSharedCounts'),
                                ('SharedAtomicControl', 'AtomicSharedCounts')):
            text = program.replace('SharedLocalControl', control).replace('COUNTS', counts)
            (root / 'Main.ns').write_text(text, encoding='utf-8')
            result = run([compiler, '--check-project-oop-hir', project], root)
            assert result.returncode == 0, result.stdout + result.stderr
            if os.name == 'nt':
                for level in ('O0', 'O2'):
                    exe = root / (control + level + '.exe')
                    result = run([compiler, '--emit-project-oop-exe', project, '-o', exe,
                                  '--no-artifact-cache', '-' + level], root)
                    assert result.returncode == 0, (control, level, result.stdout + result.stderr)
                    result = run([exe], root)
                    assert result.returncode == 42, (control, level, result.returncode, result.stdout, result.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
