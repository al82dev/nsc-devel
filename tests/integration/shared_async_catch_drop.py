"""Check shared owners and caught exceptions when dropping a parked async iterator."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


SOURCE = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public static int Drops;
 public Exception() { } ~Exception() { Drops = Drops + 1; } } }
class Payload { public static int Drops; public Payload() { } ~Payload() { Drops = Drops + 1; } }
class Program {
 public static AsyncIterator<int, int> Gen(FAMILY<Payload> owner) {
  try { throw new Exception(); } catch (Exception error) { yield return 7; }
 }
 public static async int Main() {
  { FAMILY<Payload> owner = SharedPointers.MakeFAMILY<Payload>();
    AsyncIterator<int, int> iterator = Gen(owner.Clone());
    bool first = await iterator.MoveNextAsync();
    if (!first || iterator.Current != 7 || owner.StrongCount() != 2 || Exception.Drops != 0) return 1;
    iterator.Drop();
    if (owner.StrongCount() != 1 || Exception.Drops != 1 || Payload.Drops != 0) return 2;
  }
  return Payload.Drops == 1 && Exception.Drops == 1 ? 42 : 3;
 }
}
'''

NAMES = ('Send', 'Sync', 'SharedStorage', 'SharedControlStorage', 'SharedReferenceCounts', 'SharedLocalControl',
         'SharedAtomicControl', 'SharedBorrow', 'SharedLocalHandle', 'SharedLocalWeak',
         'SharedLocalFactory', 'SharedAtomicHandle', 'SharedAtomicWeak', 'SharedAtomicFactory',
         'SharedView', 'Rc', 'WeakRc', 'Arc', 'WeakArc', 'SharedPointers')


def main():
    if os.name != 'nt':
        raise SystemExit('this directed test requires native Windows execution')
    compiler = Path(sys.argv[1]).resolve()
    library = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None
    sdk = Path(__file__).resolve().parents[2] / 'lib' / 'NativeSharp.Core'
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-shared-async-catch-drop-') as temporary:
        root = Path(temporary)
        if library is None:
            for name in NAMES:
                shutil.copyfile(sdk / (name + '.ns'), root / (name + '.ns'))
            references = ''
            source = SOURCE
        else:
            metadata = library.with_suffix('.nsmeta')
            if not library.is_file() or not metadata.is_file():
                raise FileNotFoundError('expected Core library and matching metadata')
            references = ('<LibraryReference Include="' + library.as_posix() +
                          '" Metadata="' + metadata.as_posix() + '"/>')
            source = SOURCE.replace('namespace NativeSharp.Core { public class Exception { public static int Drops;\n'
                                    ' public Exception() { } ~Exception() { Drops = Drops + 1; } } }\n', '')
            source = source.replace(' || Exception.Drops != 0', '')
            source = source.replace(' || Exception.Drops != 1', '')
            source = source.replace(' && Exception.Drops == 1', '')
            if 'Exception.Drops' in source or 'class Exception' in source:
                raise ValueError('binary fixture still contains the source-only exception probe')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" />' + references + '</ItemGroup></Project>',
                           encoding='utf-8')
        for family in ('Rc', 'Arc'):
            (root / 'Main.ns').write_text(source.replace('FAMILY', family), encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (family + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                        errors='replace', timeout=120)
                print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                      result.stdout + result.stderr, flush=True)
                if result.returncode:
                    failures.append((family, level, 'compile'))
                    continue
                result = subprocess.run([str(executable)], cwd=root, capture_output=True,
                                        text=True, errors='replace', timeout=120)
                print(family, level, 'exit=' + str(result.returncode), flush=True)
                if result.returncode != 42:
                    failures.append((family, level, 'run', result.returncode))
    print('FAILURES', failures, flush=True)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
