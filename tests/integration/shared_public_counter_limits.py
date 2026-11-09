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
    program = '''using NativeSharp.Core; using NativeSharp.Core.Internal;
namespace NativeSharp.Core { public class Exception { } }
class Payload { public static int Drops; public int Value; public Payload() { Value = 42; }
 ~Payload() { Drops = Drops + 1; } }
class Program { public static int Main() { int failures = 0;
 unsafe("single-threaded counter boundary fixture") {
  long[] block = SharedControlStorage.Allocate<Payload>();
  Payload payload = SharedControlStorage.Construct<Payload>(block);
  Payload* slot = (Payload*)&block[6]; *slot = payload; CONTROL.Initialize(block);
  { HANDLE<Payload> initial = new HANDLE<Payload>(block, slot);
    STRONG<Payload> owner = new STRONG<Payload>(move(initial));
    block[0] = 9223372036854775807l;
    { STRONG<Payload> clone = owner.Clone();
      if (!clone.IsEmpty() || block[0] != 9223372036854775807l) failures = failures + 1; }
    block[0] = 1l;
    { WEAK<Payload> weak = owner.Downgrade();
      if (weak.Expired() || weak.WeakCount() != 1l) failures = failures + 1;
      block[0] = 9223372036854775807l;
      { STRONG<Payload> upgraded = weak.Upgrade();
        if (!upgraded.IsEmpty() || block[0] != 9223372036854775807l) failures = failures + 1; }
      block[0] = 1l;
      block[1] = 9223372036854775807l;
      { WEAK<Payload> clone = weak.Clone();
        if (!clone.Expired() || block[1] != 9223372036854775807l) failures = failures + 1; }
      block[1] = 2l;
    }
    block[1] = 9223372036854775807l;
    { WEAK<Payload> failed = owner.Downgrade();
      if (!failed.Expired() || block[1] != 9223372036854775807l) failures = failures + 1; }
    block[1] = 1l;
    if (owner.IsEmpty() || owner.StrongCount() != 1l || owner.WeakCount() != 0l ||
        owner.Get().Value != 42 || Payload.Drops != 0) failures = failures + 1;
  }
  if (Payload.Drops != 1) failures = failures + 1;
 }
 return failures == 0 ? 42 : failures;
} }'''
    with tempfile.TemporaryDirectory(prefix='nsc-public-counter-limit-') as temporary:
        root = Path(temporary)
        for name in names:
            shutil.copyfile(repo / 'lib' / 'NativeSharp.Core' / (name + '.ns'), root / (name + '.ns'))
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        for strong, weak, handle, control in (('Rc', 'WeakRc', 'SharedLocalHandle', 'SharedLocalControl'),
                                              ('Arc', 'WeakArc', 'SharedAtomicHandle', 'SharedAtomicControl')):
            source = (program.replace('STRONG', strong).replace('WEAK', weak)
                      .replace('HANDLE', handle).replace('CONTROL', control))
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / (strong + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-o', exe, '-' + level], root)
                assert result.returncode == 0, (strong, level, result.stdout + result.stderr)
                result = run([exe], root)
                assert result.returncode == 42, (strong, level, result.returncode, result.stdout, result.stderr)
                print(strong, level, 'exit=42', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
