import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    compiler = Path(sys.argv[1]).resolve()
    core = Path(sys.argv[2]).resolve()
    cases = (
        ('Send', 'Safe', True), ('Sync', 'Safe', True),
        ('Send', 'Raw', False), ('Sync', 'Raw', False),
        ('Send', 'SendOnly', True), ('Sync', 'SendOnly', False),
        ('Send', 'RefMutRaw', False), ('Sync', 'RefMutRaw', False),
        ('Send', 'Indirect', False), ('Sync', 'Indirect', False),
    )
    program = '''using NativeSharp.Core;
struct Safe { public int value; }
struct Raw { public int* value; }
struct SendOnly { public ref mut int value; }
struct RefMutRaw { public ref mut Raw value; }
struct Indirect { public RefMutRaw nested; }
class Program { public static int Require<T>() where T : MARKER { return 42; }
 public static int Main() { return Require<TYPE>(); } }
'''
    with tempfile.TemporaryDirectory(prefix='nsc-send-mut-ref-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        reference = ('<LibraryReference Include="' + core.as_posix() + '" Metadata="' +
                     core.with_suffix('.nsmeta').as_posix() + '" />' if core.suffix.lower() == '.lib' else
                     '<ProjectReference Include="' + core.as_posix() + '" />')
        project.write_text('<Project><PropertyGroup><Name>SendMutRef</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />' + reference +
                           '</ItemGroup></Project>', encoding='utf-8')
        for marker, value_type, accepted in cases:
            (root / 'Main.ns').write_text(program.replace('MARKER', marker).replace('TYPE', value_type),
                                          encoding='utf-8')
            command = [str(compiler), '--check-project-method-bodies', str(project)]
            result = subprocess.run(command, cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if accepted:
                assert result.returncode == 0, (marker, value_type, result.stdout)
            else:
                assert result.returncode != 0 and 'does not implement it' in result.stdout, (
                    marker, value_type, result.stdout)
            print(marker, value_type, 'accepted' if accepted else 'rejected', flush=True)


if __name__ == '__main__':
    main()
