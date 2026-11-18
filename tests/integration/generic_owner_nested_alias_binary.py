"""Keep nested generic identities distinct across source aliases and library boundaries."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, root):
    result = subprocess.run([str(item) for item in command], cwd=root, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline([str(item) for item in command]),
          'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-nested-alias-') as temporary:
        root = Path(temporary)
        (root / 'Types.ns').write_text('''namespace First { public class Value {} }
namespace Second { public class Value {} }
namespace Fixture {
public struct Nest<T> { public T Get(T value) { return value; } }
public struct Owner<T> { public T Echo<U>(T value, U ignored) { return value; } }
}''', encoding='utf-8')
        (root / 'One.ns').write_text('''using Value = First.Value;
namespace Fixture { public class One { public Owner<Nest<Value>> field; } }''', encoding='utf-8')
        (root / 'Two.ns').write_text('''using Value = Second.Value;
namespace Fixture { public class Two { public Owner<Nest<Value>> field; } }''', encoding='utf-8')
        (root / 'Consumer.ns').write_text('''using Fixture;
class Program { public static int Main() {
 One a = new One(); Two b = new Two();
 Nest<First.Value> x; Nest<Second.Value> y;
 Nest<First.Value> left = a.field.Echo<int>(x, 1);
 Nest<Second.Value> right = b.field.Echo<int>(y, 1);
 return 42; } }''', encoding='utf-8')
        producer = root / 'Producer.nsproj'
        consumer = root / 'Consumer.nsproj'
        for dynamic in (False, True):
            producer.write_text('<Project><PropertyGroup><Name>AliasProducer</Name><OutputType>' +
                                ('SharedLibrary' if dynamic else 'StaticLibrary') +
                                '</OutputType></PropertyGroup><ItemGroup><Compile Include="Types.ns"/>'
                                '<Compile Include="One.ns"/><Compile Include="Two.ns"/>'
                                '</ItemGroup></Project>', encoding='utf-8')
            dump = run([compiler, '--dump-project-generics', producer], root)
            for name in ('First.Value', 'Second.Value'):
                expected = 'Owner`1<generic:AliasProducer::Fixture.Nest`1<type:AliasProducer::' + name + '>>'
                if dump.returncode or expected not in dump.stdout:
                    failures.append((dynamic, name, 'identity'))
            library = root / ('dynamic.lib' if dynamic else 'static.lib')
            output = library.with_suffix('.dll') if dynamic else library
            emit = '--emit-project-oop-dll' if dynamic else '--emit-project-oop-lib'
            if run([compiler, emit, producer, '-O0', '-o', output, '--no-artifact-cache'], root).returncode:
                failures.append((dynamic, 'producer'))
                continue
            consumer.write_text('<Project><PropertyGroup><Name>AliasConsumer</Name></PropertyGroup><ItemGroup>'
                                '<Compile Include="Consumer.ns"/><LibraryReference Include="' +
                                library.as_posix() + '" Metadata="' + library.with_suffix('.nsmeta').as_posix() +
                                '"/></ItemGroup></Project>', encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / (('dynamic-' if dynamic else 'static-') + level + '.exe')
                if run([compiler, '--emit-project-oop-exe', consumer, '-' + level, '-o', exe,
                        '--no-artifact-cache'], root).returncode:
                    failures.append((dynamic, level, 'consumer'))
                elif run([exe], root).returncode != 42:
                    failures.append((dynamic, level, 'execute'))
    if failures:
        print('FAIL', failures, file=sys.stderr, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
