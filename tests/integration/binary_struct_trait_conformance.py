"""Check producer-owned struct trait conformance across binary library boundaries."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, root):
    result = subprocess.run([str(x) for x in command], cwd=root, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline([str(x) for x in command]),
          'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-binary-struct-trait-') as temporary:
        root = Path(temporary)
        (root / 'Producer.ns').write_text('''namespace Fixture {
public trait Reducer<T> { T Combine(T left, T right); }
public struct IntReducer { }
impl Reducer<int> for IntReducer { int Combine(int left, int right) { return left + right; } }
public struct Other { }
public class Api { public int Apply<U>(U op) where U : Reducer<int> { return op.Combine(20, 22); } }
}''', encoding='utf-8')
        producer = root / 'Producer.nsproj'
        consumer = root / 'Consumer.nsproj'
        valid = '''using Fixture;
class Program { public static int Main() { Api api = new Api(); IntReducer op;
 return api.Apply<IntReducer>(op); } }'''
        invalid = '''using Fixture;
class Program { public static int Main() { Api api = new Api(); Other op;
 return api.Apply<Other>(op); } }'''
        for dynamic in (False, True):
            producer.write_text('<Project><PropertyGroup><Name>TraitProducer</Name><OutputType>' +
                                ('SharedLibrary' if dynamic else 'StaticLibrary') +
                                '</OutputType></PropertyGroup><ItemGroup><Compile Include="Producer.ns"/>'
                                '</ItemGroup></Project>', encoding='utf-8')
            for producer_level in ('O0', 'O2'):
                stem = ('dynamic-' if dynamic else 'static-') + producer_level
                library = root / (stem + '.lib')
                output = library.with_suffix('.dll') if dynamic else library
                if run([compiler, '--emit-project-oop-dll' if dynamic else '--emit-project-oop-lib',
                        producer, '-' + producer_level, '-o', output, '--no-artifact-cache'], root).returncode:
                    failures.append((stem, 'producer'))
                    continue
                consumer.write_text('<Project><PropertyGroup><Name>TraitConsumer</Name></PropertyGroup>'
                                    '<ItemGroup><Compile Include="Consumer.ns"/><LibraryReference Include="' +
                                    library.as_posix() + '" Metadata="' + library.with_suffix('.nsmeta').as_posix() +
                                    '"/></ItemGroup></Project>', encoding='utf-8')
                for level in ('O0', 'O2'):
                    exe = root / (stem + '-' + level + '.exe')
                    (root / 'Consumer.ns').write_text(valid, encoding='utf-8')
                    built = run([compiler, '--emit-project-oop-exe', consumer, '-' + level,
                                 '-o', exe, '--no-artifact-cache'], root)
                    if built.returncode or run([exe], root).returncode != 42:
                        failures.append((stem, level, 'valid'))
                    (root / 'Consumer.ns').write_text(invalid, encoding='utf-8')
                    rejected = run([compiler, '--emit-project-oop-exe', consumer, '-' + level,
                                    '-o', exe, '--no-artifact-cache'], root)
                    if not rejected.returncode or 'does not implement it' not in rejected.stdout + rejected.stderr:
                        failures.append((stem, level, 'invalid'))
    if failures:
        print('FAIL', failures, file=sys.stderr, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
