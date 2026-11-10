"""Check that shared-owner count observation remains available during a view."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(part) for part in command], cwd=cwd,
                          capture_output=True, text=True, errors='replace')


def main():
    compiler, library = map(lambda value: Path(value).resolve(), sys.argv[1:3])
    metadata = library.with_suffix('.nsmeta')
    assert library.is_file() and metadata.is_file()
    with tempfile.TemporaryDirectory(prefix='nsc-shared-count-view-') as temporary:
        root = Path(temporary)
        project = root / 'Consumer.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Consumer</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" />'
                           '<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                           metadata.as_posix() + '" /></ItemGroup></Project>', encoding='utf-8')
        source = root / 'Program.ns'
        for family in ('Rc', 'Arc'):
            source.write_text(('''using NativeSharp.Core;
class Payload { public long Data; public Payload() { Data = 42l; } }
class Program { public static int Main() { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = owner.Borrow();
 return owner.StrongCount() == 1l && view.Get().Data == 42l ? 42 : 1; } }
''').replace('STRONG', family).replace('MAKE', 'Make' + family), encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (family + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                              '-o', executable, '--no-artifact-cache'], root)
                assert result.returncode == 0, (family, level, result.stdout + result.stderr)
                result = run([executable], root)
                assert result.returncode == 42, (family, level, result.returncode)
                print(family, level, 'exit=42', flush=True)


if __name__ == '__main__':
    main()
