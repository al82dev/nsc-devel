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


if __name__ == '__main__':
    main()
