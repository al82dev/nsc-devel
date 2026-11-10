from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1]).resolve()
    library = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None
    sdk = Path(__file__).resolve().parents[2] / 'lib' / 'NativeSharp.Core'
    names = ('SharedStorage', 'SharedControlStorage', 'SharedReferenceCounts', 'SharedLocalControl',
             'SharedAtomicControl', 'SharedBorrow', 'SharedLocalHandle', 'SharedLocalWeak',
             'SharedLocalFactory', 'SharedAtomicHandle', 'SharedAtomicWeak', 'SharedAtomicFactory',
             'SharedView', 'Rc', 'WeakRc', 'Arc', 'WeakArc', 'SharedPointers')
    with tempfile.TemporaryDirectory(prefix='nsc-shared-weakcount-view-') as temporary:
        root = Path(temporary)
        if library is None:
            for name in names:
                shutil.copyfile(sdk / (name + '.ns'), root / (name + '.ns'))
            references = ''
        else:
            metadata = library.with_suffix('.nsmeta')
            assert library.is_file() and metadata.is_file()
            references = ('<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                          metadata.as_posix() + '" />')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="*.ns" />' + references +
                           '</ItemGroup></Project>', encoding='utf-8')
        for family in ('Rc', 'Arc'):
            source = '''using NativeSharp.Core;
class Payload { public long Data; public Payload() { Data = 42l; } }
class Program { public static int Main() { FAMILY<Payload> owner = SharedPointers.MAKE<Payload>();
 WEAK<Payload> weak = owner.Downgrade(); SharedView<Payload> view = owner.Borrow();
 return owner.StrongCount() == 1l && owner.WeakCount() == 1l && view.Get().Data == 42l ? 42 : 1; } }
'''.replace('FAMILY', family).replace('WEAK', 'Weak' + family).replace('MAKE', 'Make' + family)
            (root / 'Program.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (family + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                build = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                assert build.returncode == 0, (family, level, build.stdout + build.stderr)
                result = subprocess.run([str(executable)], cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                assert result.returncode == 42, (family, level, result.returncode)
                print(family, level, 'exit=42', flush=True)


if __name__ == '__main__':
    main()
