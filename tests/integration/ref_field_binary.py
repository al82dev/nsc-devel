import subprocess
import sys
import tempfile
from pathlib import Path


def run(command, cwd):
    return subprocess.run([str(arg) for arg in command], cwd=cwd, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def main():
    compiler = Path(sys.argv[1]).resolve()
    core = Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-ref-field-binary-') as temporary:
        root = Path(temporary)
        reference = '<LibraryReference Include="' + core.as_posix() + '" Metadata="' + core.with_suffix('.nsmeta').as_posix() + '" />'
        producer = root / 'Producer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>RefProducer</Name><Language>3</Language>'
                            '<OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup>'
                            '<Compile Include="Producer.ns" />' + reference + '</ItemGroup></Project>', encoding='utf-8')
        (root / 'Producer.ns').write_text('public struct Holder { public ref int Field; }', encoding='utf-8')
        library = root / 'Producer.lib'
        result = run([compiler, '--emit-project-oop-lib', producer, '-O0', '-o', library, '--no-artifact-cache'], root)
        assert result.returncode == 0 and library.with_suffix('.nsmeta').is_file(), ('producer', result.stdout)
        for mode, producer_reference in (
                ('source', '<ProjectReference Include="' + producer.as_posix() + '" />'),
                ('binary', '<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                 library.with_suffix('.nsmeta').as_posix() + '" />')):
            project = root / (mode + '.nsproj')
            project.write_text('<Project><PropertyGroup><Name>RefConsumer</Name><Language>3</Language>'
                               '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />' + reference +
                               producer_reference + '</ItemGroup></Project>', encoding='utf-8')
            source = root / 'Main.ns'
            source.write_text('''class Program { public static int Main() {
 int value = 42; Holder holder = new Holder(); holder.Field = ref value; return holder.Field;
} }''', encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / (mode + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', exe,
                              '--no-artifact-cache'], root)
                assert result.returncode == 0, (mode, level, result.stdout)
                result = run([exe], root)
                assert result.returncode == 42, (mode, level, result.returncode, result.stdout)
                print(mode, level, 'ref field read exit=42', flush=True)
            source.write_text('''class Program { public static Holder Escape() {
 int value = 42; Holder holder = new Holder(); holder.Field = ref value; return holder;
} public static int Main() { return 0; } }''', encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', project], root)
            assert result.returncode != 0 and 'cannot return a value whose ref field' in result.stdout, (mode, result.stdout)
            print(mode, 'ref field escape rejected', flush=True)


if __name__ == '__main__':
    main()
