"""Keep producer-owned struct trait conformance across NativeSharp libraries."""
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
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-owner-method-binary-') as temporary:
        root = Path(temporary)
        (root / 'Producer.ns').write_text('''namespace Fixture {
public struct Pair<const int A, const int B> { public int Sum() { return A + B; } }
public struct Box<const int N> {
 public Pair<N, M> Make<const int M>() { Pair<N, M> p; return p; }
 public U Echo<U>(U value) { return value; }
 public U Echo<U>(U[] values) { return values[0]; }
 public static U Static<U>(U value) { return value; }
 public int Internal() { return Echo<int>(42); }
}
public class Box { public U Echo<U>(U value) { return value; } }
public trait Reducer<T> { T Combine(T left, T right); }
public struct IntReducer { }
impl Reducer<int> for IntReducer { int Combine(int left, int right) { return left + right; } }
public struct Holder<T> { public T Keep<TOp>(T value, TOp op) where TOp : Reducer<T> { return op.Combine(value, value); } }
}''', encoding='utf-8')
        valid_source = '''using Fixture;
class Program { public static int Main() { Box<20> box; Pair<20, 22> p = box.Make<22>();
 Box plain = new Box(); int[] values = new int[1]; values[0] = p.Sum();
 int a = box.Echo<int>(values); int b = box.Echo(values); delete[] values;
 Holder<int> h; IntReducer op; int c = h.Keep(21, op);
 return plain.Echo(a) == 42 && b == 42 && c == 42 && Box<20>.Static(42) == 42 && box.Internal() == 42 ? 42 : 1; } }'''
        invalid_source = '''using Fixture;
class Program { public static int Main() { Holder<long> h; IntReducer op; long value = 42;
 return (int)h.Keep(value, op); } }'''
        producer = root / 'Producer.nsproj'
        consumer = root / 'Consumer.nsproj'
        for dynamic in (False, True):
            producer.write_text('<Project><PropertyGroup><Name>OwnerProducer</Name>'
                                '<OutputType>' + ('SharedLibrary' if dynamic else 'StaticLibrary') +
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
                consumer.write_text('<Project><PropertyGroup><Name>OwnerConsumer</Name></PropertyGroup>'
                                    '<ItemGroup><Compile Include="Consumer.ns"/><LibraryReference Include="' +
                                    library.as_posix() + '" Metadata="' + library.with_suffix('.nsmeta').as_posix() +
                                    '"/></ItemGroup></Project>', encoding='utf-8')
                for level in ('O0', 'O2'):
                    exe = root / (stem + '-' + level + '.exe')
                    (root / 'Consumer.ns').write_text(valid_source, encoding='utf-8')
                    if run([compiler, '--emit-project-oop-exe', consumer, '-' + level, '-o', exe,
                            '--no-artifact-cache'], root).returncode:
                        failures.append((stem, level, 'consumer'))
                    elif run([exe], root).returncode != 42:
                        failures.append((stem, level, 'execute'))
                    (root / 'Consumer.ns').write_text(invalid_source, encoding='utf-8')
                    negative = run([compiler, '--emit-project-oop-exe', consumer, '-' + level,
                                    '-o', exe, '--no-artifact-cache'], root)
                    if not negative.returncode or 'does not implement it' not in negative.stdout + negative.stderr:
                        failures.append((stem, level, 'dependent constraint'))
    if failures:
        print('FAIL', failures, file=sys.stderr, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
