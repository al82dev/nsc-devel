import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(item) for item in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler = Path(sys.argv[1])
    with tempfile.TemporaryDirectory(prefix='nsc-generic-alias-isolation-') as temporary:
        root = Path(temporary)
        source_a = '''using Pick = Provider.One;
namespace Provider {
 public static class ApiA { public static int Value<T>() where T : new() { return Pick.Helper.Read(); } }
}
namespace Provider.One { internal static class Helper { public static int Read() { return 21; } } }
'''
        source_b = '''using Pick = Provider.Two;
namespace Provider {
 public static class ApiB { public static int Value<T>() where T : new() { return Pick.Helper.Read(); } }
}
namespace Provider.Two { internal static class Helper { public static int Read() { return 33; } } }
'''
        if os.environ.get('NSC_ALIAS_NAMESPACE_BLOCKS') == '1':
            # Reopened namespace blocks in one unit are the shape the metadata emitter uses.
            source_a = source_a.replace('using Pick = Provider.One;\nnamespace Provider {',
                                        'namespace Provider {\n using Pick = Provider.One;')
            source_b = source_b.replace('using Pick = Provider.Two;\nnamespace Provider {',
                                        'namespace Provider {\n using Pick = Provider.Two;')
            source_a += source_b
            source_b = ''
        (root / 'A.ns').write_text(source_a, encoding='utf-8')
        (root / 'B.ns').write_text(source_b, encoding='utf-8')
        (root / 'Program.ns').write_text('''using Provider;
class Payload { public Payload() { } }
class Program { public static int Main() {
 return ApiA.Value<Payload>() == 21 && ApiB.Value<Payload>() == 33 ? 42 : 1;
} }
''', encoding='utf-8')
        producer = root / 'Producer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>Producer</Name><Language>3</Language>'
                            '<OutputType>Library</OutputType></PropertyGroup><ItemGroup>'
                            '<Compile Include="A.ns" /><Compile Include="B.ns" />'
                            '</ItemGroup></Project>', encoding='utf-8')
        combined = root / 'Combined.nsproj'
        combined.write_text('<Project><PropertyGroup><Name>Producer</Name><Language>3</Language>'
                            '</PropertyGroup><ItemGroup><Compile Include="A.ns" />'
                            '<Compile Include="B.ns" /><Compile Include="Program.ns" />'
                            '</ItemGroup></Project>', encoding='utf-8')
        for level in ('O0', 'O2'):
            exe = root / ('combined-' + level + '.exe')
            result = run([compiler, '--emit-project-oop-exe', combined, '-' + level, '-o', exe], root)
            assert result.returncode == 0, ('source control', level, result.stdout + result.stderr)
            result = run([exe], root)
            assert result.returncode == 42, ('source control', level, result.returncode)
            print('source control', level, 'exit=42', flush=True)
        library = root / 'Producer.lib'
        result = run([compiler, '--emit-project-oop-lib', producer, '-O0', '-o', library], root)
        assert result.returncode == 0, ('producer', result.stdout + result.stderr)
        print('producer library and metadata generated', flush=True)
        consumer = root / 'Consumer.nsproj'
        consumer.write_text('<Project><PropertyGroup><Name>Consumer</Name><Language>3</Language>'
                            '</PropertyGroup><ItemGroup><Compile Include="Program.ns" />'
                            '<LibraryReference Include="Producer.lib" Metadata="Producer.nsmeta" />'
                            '</ItemGroup></Project>', encoding='utf-8')
        for level in ('O0', 'O2'):
            exe = root / ('consumer-' + level + '.exe')
            result = run([compiler, '--emit-project-oop-exe', consumer, '-' + level, '-o', exe], root)
            assert result.returncode == 0, ('consumer', level, result.stdout + result.stderr)
            result = run([exe], root)
            assert result.returncode == 42, ('consumer', level, result.returncode)
            print('consumer', level, 'exit=42', flush=True)
        return 0


if __name__ == '__main__':
    sys.exit(main())
