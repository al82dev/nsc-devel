"""Check that public tuple signatures publish their closed generic dependencies."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, cwd):
    result = subprocess.run([str(x) for x in command], cwd=cwd, capture_output=True,
                            text=True, errors='replace', timeout=120)
    if result.returncode:
        raise AssertionError(f'{command}: exit {result.returncode}\n{result.stdout}{result.stderr}')
    return result.stdout + result.stderr


def main():
    compiler = Path(sys.argv[1]).resolve()
    core = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None
    cases = [('direct', 'SharedView<Payload>'),
             ('tuple', '(SharedView<Payload>, int)'),
             ('nested', '((SharedView<Payload>, int), int)')]
    with tempfile.TemporaryDirectory(prefix='tuple-metadata-specialization-') as temporary:
        root = Path(temporary)
        if core is None:
            dependency = root / 'dependency'
            dependency.mkdir()
            (dependency / 'Core.nsproj').write_text(
                '<Project><PropertyGroup><Name>NativeSharpCore</Name><Language>3</Language>'
                '</PropertyGroup><ItemGroup><Compile Include="Core.ns" /></ItemGroup></Project>', encoding='utf-8')
            (dependency / 'Core.ns').write_text(
                'namespace NativeSharp.Core { public struct SharedView<T> { public int Value; } }', encoding='utf-8')
        for name, parameter in cases:
            folder = root / name
            folder.mkdir()
            (folder / 'Main.ns').write_text(
                'using NativeSharp.Core; public class Payload { public int Data; '
                'public Payload() { Data = 42; } } public class Reader { '
                'public static int Read(' + parameter + ' value) { return 42; } }', encoding='utf-8')
            project = folder / 'Producer.nsproj'
            reference = ('<ProjectReference Include="../dependency/Core.nsproj" />' if core is None else
                '<LibraryReference Include="' + core.as_posix() + '" Metadata="' +
                core.with_suffix('.nsmeta').as_posix() + '" />')
            project.write_text('<Project><PropertyGroup><Name>SpecializationProbe</Name><Language>3</Language>'
                '<OutputType>Library</OutputType></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                + reference + '</ItemGroup></Project>', encoding='utf-8')
            library = folder / 'Probe.lib'
            run([compiler, '--emit-project-oop-lib', project, '-O0', '-o', library,
                 '--no-artifact-cache'], folder)
            output = run([compiler, '--verify-library-metadata', library.with_suffix('.nsmeta')], folder)
            if 'generic-specializations: 1\n' not in output.replace('\r\n', '\n'):
                raise AssertionError(f'{name}: expected one specialization\n{output}')
            metadata = library.with_suffix('.nsmeta').read_bytes()
            expected = b'NativeSharpCore::NativeSharp.Core.SharedView`1<type:SpecializationProbe::Payload>'
            if expected not in metadata:
                raise AssertionError(f'{name}: specialization identity missing from metadata')
            print(name, 'specialization published and metadata verified', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
