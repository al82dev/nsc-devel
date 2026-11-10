"""Directed semantic checks for shared pointer captures by a thread executor."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1]).resolve()
    options = sys.argv[2:]
    prototype = '--prototype-contract' in options
    arguments = [value for value in options if value != '--prototype-contract']
    library = Path(arguments[0]).resolve() if arguments else None
    if prototype and library is not None:
        raise ValueError('prototype contracts require the isolated source fixture')
    core_project = Path(__file__).resolve().parents[2] / 'lib/NativeSharp.Core/NativeSharp.Core.nsproj'
    source = '''using NativeSharp.Core;
struct Safe { public int value; }
struct Raw { public byte* value; }
struct SendOnly { public ref mut int value; }
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
    cases = {'Rc<Safe>': False, 'WeakRc<Safe>': False,
             'Arc<Safe>': True, 'WeakArc<Safe>': True,
             'Arc<Arc<Safe>>': True, 'Arc<WeakArc<Safe>>': True,
             'Arc<Rc<Safe>>': False, 'Arc<Arc<Raw>>': False,
             'Arc<Raw>': False, 'WeakArc<Raw>': False,
             'Arc<SendOnly>': False, 'WeakArc<SendOnly>': False}
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-arc-thread-capture-') as temporary:
        root = Path(temporary)
        if library is None:
            if prototype:
                isolated_core = root / 'Core'
                shutil.copytree(core_project.parent, isolated_core,
                                ignore=shutil.ignore_patterns('obj', 'bin', '.git'))
                for name in ('Arc', 'WeakArc'):
                    with (isolated_core / (name + '.ns')).open('a', encoding='utf-8') as stream:
                        stream.write('\nnamespace NativeSharp.Core {\n'
                                     'unsafe impl<T> Send for ' + name + '<T> where T : Send, Sync {}\n'
                                     'unsafe impl<T> Sync for ' + name + '<T> where T : Send, Sync {}\n}\n')
                core_project = isolated_core / 'NativeSharp.Core.nsproj'
            reference = '<ProjectReference Include="' + core_project.as_posix() + '" />'
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
            command = [str(compiler), '--check-project-method-bodies', str(project)]
            result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            output = result.stdout + result.stderr
            accepted = result.returncode == 0
            correct = accepted == expected and (accepted or 'not Send' in output or 'to be Sync' in output)
            print(target, 'accepted' if accepted else 'rejected',
                  'expected=' + ('accept' if expected else 'reject'), flush=True)
            if not correct:
                failures.append((target, result.returncode, output))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
