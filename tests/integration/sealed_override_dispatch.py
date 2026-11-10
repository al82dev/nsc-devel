"""A sealed override may be called directly; an open base remains virtual."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


SOURCE = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Base { public virtual int Value() { return 1; } }
class Derived : Base { public sealed override int Value() { return 42; } }
class Further : Derived { }
class Other : Base { public override int Value() { return 3; } }
class ThrowBase { public virtual int Value() throws Exception { return 0; } }
class ThrowDerived : ThrowBase { public sealed override int Value() throws Exception { throw new Exception(); } }
class Program {
 public static int Sealed(Derived value) { return value.Value(); }
 public static int Open(Base value) { return value.Value(); }
 public static int ThrowSealed(ThrowDerived value) throws Exception { return value.Value(); }
 public static int Main() { raii Further first = new Further(); raii Other second = new Other();
  raii ThrowDerived third = new ThrowDerived(); int caught = 0;
  try { int value = ThrowSealed(third); } catch (Exception error) { caught = 1; }
  return Sealed(first) == 42 && Open(second) == 3 && caught == 1 ? 42 : 1; }
}
'''


def run(command, cwd):
    result = subprocess.run([str(item) for item in command], cwd=cwd, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline([str(item) for item in command]),
          'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
    return result


def main():
    if os.name != 'nt':
        raise SystemExit('native Windows execution required')
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-sealed-dispatch-') as temporary:
        root = Path(temporary)
        (root / 'Main.ns').write_text(SOURCE, encoding='utf-8')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>SealedDispatchProbe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        result = run([compiler, '--dump-project-oop-mir', project], root)
        assert result.returncode == 0
        functions = result.stdout.split('\nfunction #')
        sealed = next(part for part in functions if 'Program__Sealed__' in part)
        opened = next(part for part in functions if 'Program__Open__' in part)
        assert 'direct-instance-call' in sealed and 'virtual-instance-call' not in sealed
        assert 'virtual-instance-call' in opened
        for level in ('O0', 'O2'):
            executable = root / (level + '.exe')
            result = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                          '-o', executable, '--no-artifact-cache'], root)
            assert result.returncode == 0
            assert run([executable], root).returncode == 42
        null_exits = {}
        for name, call in (('sealed', 'Sealed(null)'), ('open', 'Open(null)')):
            source = SOURCE.replace(
                'return Sealed(first) == 42 && Open(second) == 3 && caught == 1 ? 42 : 1;',
                'return ' + call + ';')
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / ('null-' + name + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                              '-o', executable, '--no-artifact-cache'], root)
                assert result.returncode == 0
                null_exits[name, level] = run([executable], root).returncode
        for level in ('O0', 'O2'):
            assert null_exits['sealed', level] != 0
            assert null_exits['sealed', level] == null_exits['open', level]
    return 0


if __name__ == '__main__':
    sys.exit(main())
