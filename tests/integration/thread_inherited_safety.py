import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    compiler = Path(sys.argv[1]).resolve()
    core = Path(sys.argv[2]).resolve()
    declarations = '''using NativeSharp.Core;
class RawBase { public int* value; }
class RawChild : RawBase { }
class RawGrandchild : RawChild { }
class SafeBase { public int value; }
class SafeChild : SafeBase { }
class MutBase { public ref mut int value; }
class MutChild : MutBase { }
struct ThreadExec : IThreadExecutor {
 public int Spawn(Future<void> task) { return 0; }
 public int PollJoin(int handle) { return 0; }
 public void Cancel(int handle) { }
}
'''
    types = ('RawBase', 'RawChild', 'RawGrandchild', 'SafeBase', 'SafeChild', 'MutBase', 'MutChild')
    with tempfile.TemporaryDirectory(prefix='nsc-thread-inherited-') as temporary:
        root = Path(temporary)
        reference = ('<LibraryReference Include="' + core.as_posix() + '" Metadata="' +
                     core.with_suffix('.nsmeta').as_posix() + '" />' if core.suffix.lower() == '.lib' else
                     '<ProjectReference Include="' + core.as_posix() + '" />')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>InheritedSafety</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />' + reference +
                           '</ItemGroup></Project>', encoding='utf-8')
        for marker in ('Send', 'Sync'):
            for spelling in types:
                bad = spelling.startswith('Raw') or (marker == 'Sync' and spelling.startswith('Mut'))
                source = declarations + '''class Program {
 public static int Require<T>() where T : MARKER { return 42; }
 public static int Main() { return Require<TYPE>(); } }
'''.replace('MARKER', marker).replace('TYPE', spelling)
                (root / 'Main.ns').write_text(source, encoding='utf-8')
                result = subprocess.run([str(compiler), '--check-project-method-bodies', str(project)],
                                        cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if bad:
                    assert result.returncode != 0 and 'does not implement it' in result.stdout, (marker, spelling, result.stdout)
                else:
                    assert result.returncode == 0, (marker, spelling, result.stdout)
                print('constraint', marker, spelling, 'rejected' if bad else 'accepted', flush=True)
        for spelling in types:
            diagnostic = 'not Send' if spelling.startswith('Raw') else 'to be Sync' if spelling.startswith('Mut') else None
            source = declarations + '''class Program {
 public static async void Child(ref TYPE value) { yield; }
 public static int Main() { TYPE value = default(TYPE);
  TaskScope<ThreadExec> scope = TaskScope<ThreadExec>.Create(new ThreadExec());
  scope.Spawn(Child(ref value)); scope.Join(); return 0; } }
'''.replace('TYPE', spelling)
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            result = subprocess.run([str(compiler), '--check-project-method-bodies', str(project)],
                                    cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if diagnostic:
                assert result.returncode != 0 and diagnostic in result.stdout, (spelling, result.stdout)
            else:
                assert result.returncode == 0, (spelling, result.stdout)
            print('capture', spelling, 'rejected' if diagnostic else 'accepted', flush=True)

        producer_project = root / 'Producer.nsproj'
        producer_project.write_text('<Project><PropertyGroup><Name>SafetyBaseProducer</Name><Language>3</Language>'
                                    '<OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup>'
                                    '<Compile Include="Producer.ns" />' + reference + '</ItemGroup></Project>',
                                    encoding='utf-8')
        library = root / 'Producer.lib'
        project.write_text('<Project><PropertyGroup><Name>InheritedSafety</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />' + reference +
                           '<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                           library.with_suffix('.nsmeta').as_posix() + '" /></ItemGroup></Project>',
                           encoding='utf-8')
        for visibility in ('public', 'private'):
            (root / 'Producer.ns').write_text('public class RawBase { ' + visibility + ' int* value; }\n'
                                              'public class SafeBase { public int value; }\n', encoding='utf-8')
            result = subprocess.run([str(compiler), '--emit-project-oop-lib', str(producer_project), '-O0',
                                     '-o', str(library), '--no-artifact-cache'], cwd=root, text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            assert result.returncode == 0 and library.with_suffix('.nsmeta').is_file(), (visibility, result.stdout)
            binary_declarations = '''using NativeSharp.Core;
class RawChild : RawBase { }
class SafeChild : SafeBase { }
struct ThreadExec : IThreadExecutor {
 public int Spawn(Future<void> task) { return 0; }
 public int PollJoin(int handle) { return 0; }
 public void Cancel(int handle) { }
}
'''
            for marker in ('Send', 'Sync'):
                for spelling in ('RawBase', 'RawChild', 'SafeBase', 'SafeChild'):
                    source = binary_declarations + '''class Program {
 public static int Require<T>() where T : MARKER { return 42; }
 public static int Main() { return Require<TYPE>(); } }
'''.replace('MARKER', marker).replace('TYPE', spelling)
                    (root / 'Main.ns').write_text(source, encoding='utf-8')
                    result = subprocess.run([str(compiler), '--check-project-method-bodies', str(project)],
                                            cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    if spelling.startswith('Raw'):
                        assert result.returncode != 0 and 'does not implement it' in result.stdout, (visibility, marker, spelling, result.stdout)
                    else:
                        assert result.returncode == 0, (visibility, marker, spelling, result.stdout)
                    print('binary base constraint', visibility, marker, spelling, 'verified', flush=True)
            for spelling in ('RawBase', 'RawChild', 'SafeBase', 'SafeChild'):
                source = binary_declarations + '''class Program {
 public static async void Child(ref TYPE value) { yield; }
 public static int Main() { TYPE value = default(TYPE);
  TaskScope<ThreadExec> scope = TaskScope<ThreadExec>.Create(new ThreadExec());
  scope.Spawn(Child(ref value)); scope.Join(); return 0; } }
'''.replace('TYPE', spelling)
                (root / 'Main.ns').write_text(source, encoding='utf-8')
                result = subprocess.run([str(compiler), '--check-project-method-bodies', str(project)],
                                        cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if spelling.startswith('Raw'):
                    assert result.returncode != 0 and 'not Send' in result.stdout, (visibility, spelling, result.stdout)
                else:
                    assert result.returncode == 0, (visibility, spelling, result.stdout)
                print('binary base capture', visibility, spelling, 'verified', flush=True)


if __name__ == '__main__':
    main()
