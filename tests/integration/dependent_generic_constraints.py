import subprocess
import sys
import tempfile
from pathlib import Path


def run(args, cwd):
    return subprocess.run([str(arg) for arg in args], cwd=cwd, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def main():
    compiler = Path(sys.argv[1]).resolve()
    common = '''public trait Reducer<T> { T Combine(T left, T right); }
public struct IntReducer { }
impl Reducer<int> for IntReducer { int Combine(int left, int right) { return left + right; } }
'''
    cases = {
        'good': (common + '''public class Program {
 private static T Keep<T, TOp>(T value, TOp op) where TOp : Reducer<T> { return value; }
 public static int Main() { IntReducer op; return Keep<int, IntReducer>(42, op); }
}''', None),
        'invoke': (common + '''public class Program {
 private static T Apply<T, TOp>(T value, TOp op) where TOp : Reducer<T> { return op.Combine(value, value); }
 public static int Main() { IntReducer op; return Apply<int, IntReducer>(21, op); }
}''', None),
        'bad': (common + '''public class Program {
 private static T Keep<T, TOp>(T value, TOp op) where TOp : Reducer<T> { return value; }
 public static int Main() { IntReducer op; return (int)Keep<long, IntReducer>(42, op); }
}''', 'does not implement it'),
        'missing': ('''public class Program {
 private static T Keep<T, TOp>(T value, TOp op) where TOp : Missing<T> { return value; }
 public static int Main() { return 42; }
}''', 'does not name a unique visible generic'),
        'struct': ('''public struct Wrapper<T> { }
public class Program {
 private static T Keep<T, TOp>(T value, TOp op) where TOp : Wrapper<T> { return value; }
 public static int Main() { return 42; }
}''', 'a generic constraint names a base class or an interface'),
        'nested-good': ('''public trait Reducer<T> { }
public struct Box<T> { }
public struct IntReducer { }
impl Reducer<Box<int>> for IntReducer { }
public class Program {
 private static T Keep<T, TOp>(T value, TOp op) where TOp : Reducer<Box<T>> { return value; }
 public static int Main() { IntReducer op; return Keep<int, IntReducer>(42, op); }
}''', None),
        'nested-bad': ('''public trait Reducer<T> { }
public struct Box<T> { }
public struct IntReducer { }
impl Reducer<Box<int>> for IntReducer { }
public class Program {
 private static T Keep<T, TOp>(T value, TOp op) where TOp : Reducer<Box<T>> { return value; }
 public static int Main() { IntReducer op; return (int)Keep<long, IntReducer>(42, op); }
}''', 'does not implement it'),
        'two-good': ('''public trait Reducer<T> { }
public struct Both { }
impl Reducer<int> for Both { }
impl Reducer<long> for Both { }
public class Program {
 private static T Keep<T, U, TOp>(T value, TOp op) where TOp : Reducer<T>, Reducer<U> { return value; }
 public static int Main() { Both op; return Keep<int, long, Both>(42, op); }
}''', None),
        'two-bad': ('''public trait Reducer<T> { }
public struct OnlyInt { }
impl Reducer<int> for OnlyInt { }
public class Program {
 private static T Keep<T, U, TOp>(T value, TOp op) where TOp : Reducer<T>, Reducer<U> { return value; }
 public static int Main() { OnlyInt op; return Keep<int, long, OnlyInt>(42, op); }
}''', 'does not implement it'),
        'exact-duplicate': ('''public trait Reducer<T> { }
public class Program {
 private static T Keep<T, TOp>(T value, TOp op) where TOp : Reducer<T>, Reducer<T> { return value; }
 public static int Main() { return 42; }
}''', 'duplicate constraint'),
    }
    with tempfile.TemporaryDirectory(prefix='nsc-dependent-constraints-') as temporary:
        root = Path(temporary)
        source = root / 'Program.ns'
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>DependentConstraints</Name>'
                           '<Language>3</Language></PropertyGroup><ItemGroup>'
                           '<Compile Include="Program.ns" /></ItemGroup></Project>', encoding='utf-8')
        for name, (body, expected) in cases.items():
            source.write_text(body, encoding='utf-8')
            if expected:
                result = run([compiler, '--check-project-generic-constraints', project], root)
                assert result.returncode != 0 and expected in result.stdout, (name, result.stdout)
                print(name, 'rejected:', expected, flush=True)
                continue
            for level in ('O0', 'O2'):
                exe = root / (name + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', exe,
                              '--no-artifact-cache'], root)
                assert result.returncode == 0, (name, level, result.stdout)
                result = run([exe], root)
                assert result.returncode == 42, (name, level, result.returncode, result.stdout)
                print(name, level, 'exit=42', flush=True)

        for name, second, expected_duplicate in (
                ('array-distinct', '3', False),
                ('array-duplicate', '2', True),
                ('array-expression-distinct', '1+2', False),
                ('array-expression-duplicate', '1+1', True)):
            first = '1+1' if 'expression' in name else '2'
            source.write_text('''public trait Reducer<T> { }
public class Program {
 private static void Keep<TOp>(TOp op) where TOp : Reducer<int[FIRST]>, Reducer<int[SECOND]> { }
 public static int Main() { return 42; }
}'''.replace('FIRST', first).replace('SECOND', second), encoding='utf-8')
            result = run([compiler, '--check-project-syntax', project], root)
            if expected_duplicate:
                assert result.returncode != 0 and 'duplicate constraint' in result.stdout, (name, result.stdout)
            else:
                assert result.returncode == 0, (name, result.stdout)
            print(name, 'verified at syntax phase', flush=True)

        producer_source = root / 'Producer.ns'
        producer_source.write_text('''public trait Reducer<T> { T Combine(T left, T right); }
public class Library {
 public static T Keep<T, TOp>(T value, TOp op) where TOp : Reducer<T> { return value; }
}''', encoding='utf-8')
        producer_project = root / 'Producer.nsproj'
        producer_project.write_text('<Project><PropertyGroup><Name>DependentProducer</Name>'
                                    '<Language>3</Language><OutputType>StaticLibrary</OutputType>'
                                    '</PropertyGroup><ItemGroup><Compile Include="Producer.ns" />'
                                    '</ItemGroup></Project>', encoding='utf-8')
        library = root / 'Producer.lib'
        result = run([compiler, '--emit-project-oop-lib', producer_project, '-O0', '-o', library,
                      '--no-artifact-cache'], root)
        assert result.returncode == 0, ('binary producer', result.stdout)
        metadata = library.with_suffix('.nsmeta')
        assert library.is_file() and metadata.is_file(), ('missing producer artifacts', library, metadata)
        project.write_text('<Project><PropertyGroup><Name>DependentConsumer</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" />'
                           '<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                           metadata.as_posix() + '" /></ItemGroup></Project>', encoding='utf-8')
        consumer = '''public struct IntReducer { }
impl Reducer<int> for IntReducer { int Combine(int left, int right) { return left + right; } }
public class Program { public static int Main() { IntReducer op; return RETURN; } }'''
        source.write_text(consumer.replace('RETURN', 'Library.Keep<int, IntReducer>(42, op)'), encoding='utf-8')
        for level in ('O0', 'O2'):
            exe = root / ('binary-' + level + '.exe')
            result = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', exe,
                          '--no-artifact-cache'], root)
            assert result.returncode == 0, ('binary consumer', level, result.stdout)
            result = run([exe], root)
            assert result.returncode == 42, ('binary consumer', level, result.returncode, result.stdout)
            print('binary consumer', level, 'exit=42', flush=True)
        source.write_text(consumer.replace('RETURN', '(int)Library.Keep<long, IntReducer>(42, op)'), encoding='utf-8')
        result = run([compiler, '--check-project-generic-constraints', project], root)
        assert result.returncode != 0 and 'does not implement it' in result.stdout, ('binary mismatch', result.stdout)
        print('binary mismatch rejected', flush=True)


if __name__ == '__main__':
    main()
