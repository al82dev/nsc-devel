"""Thread-executor capture checks for a conditional unsafe pointer wrapper."""
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1]).resolve()
    library = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None
    core = Path(__file__).resolve().parents[2] / 'lib/NativeSharp.Core/NativeSharp.Core.nsproj'
    source = '''using NativeSharp.Core;
struct Safe { public int value; }
struct Raw { public int* value; }
struct SendOnly { public ref mut int value; }
struct SharedToSendOnly { public ref SendOnly value; }
class Recursive { public Recursive next; }
class RecursiveRaw { public RecursiveRaw next; public int* raw; }
namespace Impostor { public trait Send {} public trait Sync {} }
struct Box<T> { public T* p; }
unsafe impl<T> Send for Box<T> where T : Send, Sync {}
unsafe impl<T> Sync for Box<T> where T : Send, Sync {}
struct Unmarked<T> { public T* p; }
struct Mismatched<T> { public T* p; }
unsafe impl<T> Send for Mismatched<T> where T : Impostor.Send, Impostor.Sync {}
unsafe impl<T> Sync for Mismatched<T> where T : Impostor.Send, Impostor.Sync {}
struct ThreadExec : IThreadExecutor {
 public int Spawn(Future<void> task) { return 0; }
 public int PollJoin(int handle) { return 0; }
 public void Cancel(int handle) { }
}
class Program {
 public static async void Child(ref TARGET value) { yield; }
 public static int Main() {
  TARGET value = default(TARGET);
  TaskScope<ThreadExec> scope = TaskScope<ThreadExec>.Create(new ThreadExec());
  scope.Spawn(Child(ref value)); scope.Join(); return 0;
 }
}
'''
    cases = {'Box<Safe>': True, 'Box<Box<Safe>>': True, 'Box<Recursive>': True,
             'Box<Raw>': False, 'Box<Box<Raw>>': False, 'Box<RecursiveRaw>': False,
             'Box<SendOnly>': False, 'Box<SharedToSendOnly>': False,
             'Unmarked<Safe>': False, 'Mismatched<Safe>': False}
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-conditional-thread-capture-') as temporary:
        root = Path(temporary)
        if library is None:
            reference = '<ProjectReference Include="' + core.as_posix() + '" />'
        else:
            metadata = library.with_suffix('.nsmeta')
            assert library.is_file() and metadata.is_file()
            reference = ('<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                         metadata.as_posix() + '" />')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>CaptureProbe</Name>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />' + reference +
                           '</ItemGroup></Project>', encoding='utf-8')
        for target, expected in cases.items():
            (root / 'Main.ns').write_text(source.replace('TARGET', target), encoding='utf-8')
            result = subprocess.run([str(compiler), '--check-project-method-bodies', str(project)],
                                    cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            output = result.stdout + result.stderr
            accepted = result.returncode == 0
            correct = accepted == expected and (accepted or 'not Send' in output or 'to be Sync' in output)
            print(target, 'accepted=' + str(accepted), 'expected=' + str(expected), flush=True)
            if not correct:
                failures.append((target, result.returncode, output))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
