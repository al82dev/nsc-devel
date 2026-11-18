"""Retain exact generic method owners when pruning precompiled surfaces."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, root):
    command = [str(value) for value in command]
    result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                            errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    producer_source = '''namespace Fixture {
public class Box { public int Echo<U>(U value) { return 11; } }
public struct Box<T> { public int Echo<U>(U value) { return 31; } }
public struct Box<T, V> { public int Echo<U>(U value) { return 7; } }
public struct TypeOwner<T> { public T Echo<U>(T value, U ignored) { return value; } }
public class Wrapper { public TypeOwner<int> value; }
}'''
    consumers = {
        'pruned': '''using Fixture;
class Program { public static int Main() { Wrapper w = new Wrapper();
 return w.value.Echo<int>(42, 0); } }''',
        'both': '''using Fixture;
class Program { public static int Main() { Box ordinary = new Box(); Box<int> generic;
 Box<int, long> pair;
 if (ordinary.Echo<int>(0) != 11 || generic.Echo<int>(0) != 31 || pair.Echo<int>(0) != 7) return 1;
 return 42; } }''',
    }
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-owner-reachability-') as temporary:
        root = Path(temporary)
        (root / 'Producer.ns').write_text(producer_source, encoding='utf-8')
        producer = root / 'Producer.nsproj'
        consumer = root / 'Consumer.nsproj'
        for dynamic in (False, True):
            producer.write_text('<Project><PropertyGroup><Name>OwnerProducer</Name><OutputType>' +
                                ('SharedLibrary' if dynamic else 'StaticLibrary') +
                                '</OutputType></PropertyGroup><ItemGroup><Compile Include="Producer.ns"/>'
                                '</ItemGroup></Project>', encoding='utf-8')
            for producer_level in ('O0', 'O2'):
                stem = ('dynamic-' if dynamic else 'static-') + producer_level
                library = root / (stem + '.lib')
                output = library.with_suffix('.dll') if dynamic else library
                emit = '--emit-project-oop-dll' if dynamic else '--emit-project-oop-lib'
                if run([compiler, emit, producer, '-' + producer_level, '-o', output,
                        '--no-artifact-cache', '--no-error-log'], root).returncode:
                    failures.append((stem, 'producer'))
                    continue
                consumer.write_text('<Project><PropertyGroup><Name>OwnerConsumer</Name></PropertyGroup>'
                                    '<ItemGroup><Compile Include="Consumer.ns"/><LibraryReference Include="' +
                                    library.as_posix() + '" Metadata="' + library.with_suffix('.nsmeta').as_posix() +
                                    '"/></ItemGroup></Project>', encoding='utf-8')
                for name, source in consumers.items():
                    (root / 'Consumer.ns').write_text(source, encoding='utf-8')
                    for level in ('O0', 'O2'):
                        exe = root / (stem + '-' + name + '-' + level + '.exe')
                        if run([compiler, '--emit-project-oop-exe', consumer, '-' + level, '-o', exe,
                                '--no-artifact-cache', '--no-error-log'], root).returncode:
                            failures.append((stem, name, level, 'consumer'))
                        elif run([exe], root).returncode != 42:
                            failures.append((stem, name, level, 'execute'))
    if failures:
        print('FAIL', failures, file=sys.stderr, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
