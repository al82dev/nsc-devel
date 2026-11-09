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
    program = '''class Base { } class Good : Base { } class Wrong { }
struct Value { public int Data; }
struct Box<T> where T : Base { public T Data; }
struct Wrapper<T> { public T Data; }
class Helper { public static int Take<U>() { return 1; } }
class Factory {
 public static int Make<T>() where T : new() { return 2; }
 public specialize static int Make<T>() where T : class, new() { BODY }
}
class Program { public static int Main() { return CALL; } }
'''
    negative = ('Box<T> box; return 1;', 'new Box<T>(); return 1;',
                'Box<T>[2] box; return 1;', 'Box<T>[] box; return 1;',
                'Wrapper<Box<T>> box; return 1;', 'return Helper.Take<Box<T>>();')
    with tempfile.TemporaryDirectory(prefix='nsc-selected-constraints-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                           '</ItemGroup></Project>', encoding='utf-8')
        source = root / 'Main.ns'
        for body in negative:
            source.write_text(program.replace('BODY', body).replace('CALL', 'Factory.Make<Wrong>()'),
                              encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', project], root)
            output = result.stdout + result.stderr
            assert result.returncode != 0 and "does not derive from" in output, (body, output)
        for body in negative:
            source.write_text(program.replace('BODY', body).replace('CALL', 'Factory.Make<Good>()'),
                              encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', project], root)
            assert result.returncode == 0, (body, result.stdout + result.stderr)
        # A Value specialization selects the unconstrained fallback. Its discarded
        # class candidate contains Box<Value>, which must not produce a diagnostic.
        source.write_text(program.replace('BODY', 'Box<T> box; return 1;')
                          .replace('CALL', 'Factory.Make<Value>() + Factory.Make<Good>()'),
                          encoding='utf-8')
        result = run([compiler, '--check-project-method-bodies', project], root)
        assert result.returncode == 0, result.stdout + result.stderr
        direct_use = program.replace('BODY', 'Box<T> box; return 1;').replace(
            'return CALL;', 'Box<Value> invalid; return Factory.Make<Value>();')
        source.write_text(direct_use, encoding='utf-8')
        result = run([compiler, '--check-project-method-bodies', project], root)
        output = result.stdout + result.stderr
        assert result.returncode != 0 and 'requires a type derived from' in output, output
        different_parameters = '''class Base { } class Wrong { }
struct Box<T> where T : Base { public T Data; }
class Factory {
 public static int Make<T>(int x) where T : new() { return 2; }
 public static int Make<T>(long x) where T : class, new() { Box<T> b; return 1; }
}
class Program { public static int Main() { return Factory.Make<Wrong>(1); } }
'''
        source.write_text(different_parameters, encoding='utf-8')
        result = run([compiler, '--check-project-method-bodies', project], root)
        assert result.returncode == 0, result.stdout + result.stderr
        source.write_text(program.replace('BODY', 'Box<T> box; return 1;')
                          .replace('CALL', 'Factory.Make<Value>() + Factory.Make<Good>()'),
                          encoding='utf-8')
        if os.name == 'nt':
            for level in ('O0', 'O2'):
                exe = root / (level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-o', exe,
                              '--no-artifact-cache', '-' + level], root)
                assert result.returncode == 0, result.stdout + result.stderr
                result = run([exe], root)
                assert result.returncode == 3, (level, result.returncode, result.stdout, result.stderr)
                print(level + ' selected and discarded overloads: ok', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
