"""Check sealed and open virtual calls across a NativeSharp library boundary."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile


PRODUCER = '''namespace DispatchFixture {
 public class Base { public Base() { } public virtual int Value() { return 1; } }
 public class Derived : Base { public Derived() { } public sealed override int Value() { return 42; } }
 public class Other : Base { public Other() { } public override int Value() { return 3; } }
}'''
CONSUMER = '''using DispatchFixture;
class Program {
 public static int Sealed(Derived value) { return value.Value(); }
 public static int Open(Base value) { return value.Value(); }
 public static int Main() { raii Derived first = new Derived(); raii Other second = new Other();
  return Sealed(first) == 42 && Open(second) == 3 ? 42 : 1; }
}'''


def run(command, root):
    args = [str(item) for item in command]
    result = subprocess.run(args, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(args), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def main():
    if os.name != 'nt':
        raise SystemExit('native Windows execution required')
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-sealed-binary-') as temporary:
        root = Path(temporary)
        (root / 'Producer.ns').write_text(PRODUCER, encoding='utf-8')
        (root / 'Main.ns').write_text(CONSUMER, encoding='utf-8')
        producer = root / 'Producer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>DispatchFixture</Name>'
                            '<OutputType>StaticLibrary</OutputType></PropertyGroup>'
                            '<ItemGroup><Compile Include="Producer.ns" /></ItemGroup></Project>', encoding='utf-8')
        consumer = root / 'Consumer.nsproj'
        for producer_level in ('O0', 'O2'):
            library = root / ('producer-' + producer_level + '.lib')
            result = run([compiler, '--emit-project-oop-lib', producer, '-' + producer_level,
                          '-o', library, '--no-artifact-cache'], root)
            assert result.returncode == 0
            consumer.write_text('<Project><PropertyGroup><Name>DispatchConsumer</Name></PropertyGroup>'
                                '<ItemGroup><Compile Include="Main.ns" /><LibraryReference Include="' +
                                library.as_posix() + '" Metadata="' + library.with_suffix('.nsmeta').as_posix() +
                                '" /></ItemGroup></Project>', encoding='utf-8')
            result = run([compiler, '--dump-project-oop-mir', consumer], root)
            assert result.returncode == 0
            functions = result.stdout.split('\nfunction #')
            sealed = next(part for part in functions if 'Program__Sealed__' in part)
            opened = next(part for part in functions if 'Program__Open__' in part)
            assert 'direct-instance-call' in sealed and 'virtual-instance-call' not in sealed
            assert 'virtual-instance-call' in opened
            for level in ('O0', 'O2'):
                executable = root / (producer_level + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', consumer, '-' + level,
                              '-o', executable, '--no-artifact-cache'], root)
                assert result.returncode == 0
                assert run([executable], root).returncode == 42
    return 0


if __name__ == '__main__':
    sys.exit(main())
