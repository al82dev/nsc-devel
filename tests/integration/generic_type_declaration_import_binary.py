"""Preserve generic type imports across a NativeSharp library boundary."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, root):
    result = subprocess.run([str(value) for value in command], cwd=root, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline([str(value) for value in command]),
          'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-generic-type-import-binary-') as temporary:
        root = Path(temporary)
        (root / 'Values.ns').write_text('''namespace Alpha { public struct Item<T> {
 public int Value() { return 21; } } }
namespace Beta { public struct Item<T> { public int Value() { return 33; } } }
''', encoding='utf-8')
        (root / 'Wrapper.ns').write_text('''using Pick = Alpha;
public struct Wrapper<T> { private Pick.Item<T> item;
 public int Read() { return item.Value(); } }
''', encoding='utf-8')
        producer = root / 'Producer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>TypeImportProducer</Name>'
                            '<OutputType>StaticLibrary</OutputType></PropertyGroup>'
                            '<ItemGroup><Compile Include="Values.ns"/><Compile Include="Wrapper.ns"/>'
                            '</ItemGroup></Project>', encoding='utf-8')
        (root / 'Main.ns').write_text('''using Pick = Beta;
class Program { public static int Main() { Wrapper<int> value; return value.Read(); } }
''', encoding='utf-8')
        consumer = root / 'Consumer.nsproj'
        for producer_level in ('O0', 'O2'):
            library = root / ('Producer-' + producer_level + '.lib')
            if run([compiler, '--emit-project-oop-lib', producer, '-' + producer_level, '-o', library,
                    '--no-artifact-cache'], root).returncode:
                return 1
            consumer.write_text('<Project><PropertyGroup><Name>TypeImportConsumer</Name></PropertyGroup>'
                                '<ItemGroup><Compile Include="Main.ns"/><LibraryReference Include="' +
                                library.as_posix() + '" Metadata="' + library.with_suffix('.nsmeta').as_posix() +
                                '"/></ItemGroup></Project>', encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (producer_level + '-' + level + '.exe')
                if run([compiler, '--emit-project-oop-exe', consumer, '-' + level, '-o', executable,
                        '--no-artifact-cache'], root).returncode:
                    return 1
                if run([executable], root).returncode != 21:
                    return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
