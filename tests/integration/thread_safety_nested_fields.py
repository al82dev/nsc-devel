from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1])
    source = '''trait Send {} trait Sync {}
struct Unsafe { public int* pointer; }
struct Nested { public Unsafe value; }
struct Deep { public Nested value; }
struct Safe { public int value; }
struct SafeNested { public Safe value; }
class Program { public static int Require<T>() where T : MARKER { return 42; }
public static int Main() { return Require<TYPE>(); } }'''
    with tempfile.TemporaryDirectory(prefix='nsc-nested-thread-safety-') as tmp:
        folder = Path(tmp)
        project = folder / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        for marker in ('Send', 'Sync'):
            for name, safe in (('Unsafe', False), ('Nested', False), ('Deep', False),
                               ('Safe', True), ('SafeNested', True)):
                (folder / 'Main.ns').write_text(source.replace('MARKER', marker).replace('TYPE', name), encoding='utf-8')
                result = subprocess.run([str(compiler), '--check-project-method-bodies', str(project)],
                                        capture_output=True, text=True, errors='replace', timeout=120)
                output = result.stdout + result.stderr
                assert (result.returncode == 0) == safe, (marker, name, output)
                if not safe:
                    assert 'requires an implementation of' in output, (marker, name, output)
    return 0


if __name__ == '__main__':
    sys.exit(main())
