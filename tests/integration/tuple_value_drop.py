import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1])
    prefix = '''class Counter { public static int Order; }
struct Guard { public int Id; public Guard(int id) { Id = id; }
 ~Guard() { Counter.Order = Counter.Order * 10 + Id; } }
'''
    cases = {
        'literal': '{ (Guard, int, Guard) pair = (new Guard(1), 0, new Guard(2)); }',
        'aggregate': '{ (Guard, int, Guard) pair = { new Guard(1), 0, new Guard(2) }; }',
        'nested': '{ (Guard, (int, Guard)) pair = (new Guard(1), (0, new Guard(2))); }',
    }
    static_cases = {
        'static_scalar': 'class Holder { public static (int, long) value; }',
        'static_nested': 'class Holder { public static (int, (int, long)) value; }',
        'static_value': 'class Holder { public static (int, Guard) value; }',
        'static_generic_open': 'class Holder<T> { public static (int, T) value; }',
    }
    alias_cases = {
        'alias_local': '''using Pair = (int, long);
class Program { public static int Main() { Pair value = (40, 2l);
 var (first, second) = value; return first + (int)second; } }''',
        'alias_chain_static': '''using Pair = (int, (int, long)); using Other = Pair;
class Holder { public static Other value; }
class Program { public static int Main() { return 42; } }''',
        'alias_named_static': '''using Pair = (int first, long second);
class Holder { public static Pair value; }
class Program { public static int Main() { return 42; } }''',
        'alias_namespace_static': '''using Pair = (int, long);
namespace Stores { Pair value; }
class Program { public static int Main() { return 42; } }''',
        'namespace_alias_control': '''using Alias = Values;
namespace Values { public class Number { public static int Get() { return 42; } } }
class Program { public static int Main() { return Alias.Number.Get(); } }''',
        'alias_chain_three': '''using Pair = (int, long); using Other = Pair; using Final = Other;
class Holder { public static Final value; }
class Program { public static int Main() { Final value = (40, 2l);
 var (first, second) = value; return first + (int)second; } }''',
        'alias_chain_shadow': '''using Pair = (int, long); using Other = Pair;
namespace Inner { using Pair = (long, int); class Holder {
 public static Other value; public static int Read() { Other item = (40, 2l);
 var (first, second) = item; return first + (int)second; } } }
class Program { public static int Main() { return Inner.Holder.Read(); } }''',
    }
    with tempfile.TemporaryDirectory(prefix='nsc-tuple-value-drop-') as temp:
        root = Path(temp)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        for name, statement in cases.items():
            source = prefix + 'class Program { public static int Main() { ' + statement + \
                ' return Counter.Order == 21 ? 42 : Counter.Order; } }'
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / (name + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-o', str(exe), '-' + level]
                build = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                assert build.returncode == 0, (name, level, build.stdout, build.stderr)
                if os.name == 'nt':
                    run = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                    print(f'{name} {level} exit={run.returncode}', flush=True)
                    assert run.returncode == 42, (name, level, run.returncode, run.stdout, run.stderr)
        for name, source in alias_cases.items():
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / (name + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-o', str(exe), '-' + level]
                build = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                       errors='replace', timeout=120)
                assert build.returncode == 0, (name, level, build.stdout, build.stderr)
                if os.name == 'nt':
                    run = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True,
                                         errors='replace', timeout=120)
                    print(f'{name} {level} exit={run.returncode}', flush=True)
                    assert run.returncode == 42, (name, level, run.returncode, run.stdout, run.stderr)
        for name, declaration in static_cases.items():
            (root / 'Main.ns').write_text(prefix + declaration +
                                         'class Program { public static int Main() { return 42; } }',
                                         encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / (name + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-o', str(exe), '-' + level]
                build = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                       errors='replace', timeout=120)
                assert build.returncode == 0, (name, level, build.stdout, build.stderr)
                if os.name == 'nt':
                    run = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True,
                                         errors='replace', timeout=120)
                    print(f'{name} {level} exit={run.returncode}', flush=True)
                    assert run.returncode == 42, (name, level, run.returncode, run.stdout, run.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
