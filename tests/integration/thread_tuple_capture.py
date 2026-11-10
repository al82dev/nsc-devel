import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    compiler = Path(sys.argv[1]).resolve()
    core = Path(sys.argv[2]).resolve()
    source = '''using NativeSharp.Core;
struct Raw { public byte* value; }
struct Safe { public int value; }
struct SendOnly { public ref mut int value; }
struct WrapperRaw { public (Raw, int) value; }
struct WrapperSafe { public (Safe, int) value; }
struct WrapperSendOnly { public (SendOnly, int) value; }
struct WrapperNested { public ((Raw, int), int) value; }
struct ThreadExec : IThreadExecutor {
 public int Spawn(Future<void> task) { return 0; }
 public int PollJoin(int handle) { return 0; }
 public void Cancel(int handle) { }
}
class Program {
 public static async void Child(ref TYPE value) { yield; }
 public static int Main() { TYPE value = default(TYPE);
  TaskScope<ThreadExec> scope = TaskScope<ThreadExec>.Create(new ThreadExec());
  scope.Spawn(Child(ref value)); scope.Join(); return 0; }
}
'''
    cases = (('WrapperRaw', 'not Send'), ('WrapperSafe', None),
             ('WrapperSendOnly', 'to be Sync'), ('WrapperNested', 'not Send'))
    with tempfile.TemporaryDirectory(prefix='nsc-thread-tuple-capture-') as temporary:
        root = Path(temporary)
        reference = ('<LibraryReference Include="' + core.as_posix() + '" Metadata="' +
                     core.with_suffix('.nsmeta').as_posix() + '" />' if core.suffix.lower() == '.lib' else
                     '<ProjectReference Include="' + core.as_posix() + '" />')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>TupleCaptureProbe</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />' + reference +
                           '</ItemGroup></Project>', encoding='utf-8')
        for spelling, diagnostic in cases:
            (root / 'Main.ns').write_text(source.replace('TYPE', spelling), encoding='utf-8')
            command = [str(compiler), '--check-project-method-bodies', str(project)]
            result = subprocess.run(command, cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if diagnostic is None:
                assert result.returncode == 0, (spelling, result.stdout)
            else:
                assert result.returncode != 0 and diagnostic in result.stdout, (spelling, result.stdout)
            print(spelling, 'accepted' if diagnostic is None else 'rejected: ' + diagnostic, flush=True)


if __name__ == '__main__':
    main()
