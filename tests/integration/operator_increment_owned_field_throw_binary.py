"""Check owned field updates when an operator from a NativeSharp library throws."""
from html import escape
from pathlib import Path
import subprocess
import sys
import tempfile


PRODUCER = '''using NativeSharp.Core;
public class Box { public static int Drops; public int Value;
 public Box(int value) { Value = value; } ~Box() { Drops = Drops + 1; }
 public static int Count() { return Drops; }
 public static owned Box operator ++(Box value) throws Exception { throw new Exception(); }
 public static owned Box operator --(Box value) throws Exception { throw new Exception(); } }
public class Holder { public owned Box Item; public Holder() { Item = new Box(10); } }
'''
CONSUMER = '''using NativeSharp.Core;
class Program { public static int Main() {
 int caught = 0;
 { raii Holder holder = new Holder();
   try { OPERATION; } catch (Exception error) { caught = caught + 1; }
   if (caught != 1 || holder.Item.Value != 10 || Box.Count() != 0) return 1; }
 return Box.Count() == 1 ? 42 : 2; } }
'''
RECEIVER = 'holder'


def run(command, cwd):
    args = [str(value) for value in command]
    result = subprocess.run(args, cwd=cwd, capture_output=True, text=True,
                            errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(args), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def reference(library, metadata):
    return ('<LibraryReference Include="' + escape(library.as_posix(), quote=True) +
            '" Metadata="' + escape(metadata.as_posix(), quote=True) + '"/>')


def main():
    compiler, core, core_metadata = (Path(value).resolve() for value in sys.argv[1:4])
    if sys.argv[4:] not in ([], ['--dll']):
        raise ValueError('expected only the optional --dll argument')
    dynamic = sys.argv[4:] == ['--dll']
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-field-throw-binary-') as temporary:
        root = Path(temporary)
        (root / 'Producer.ns').write_text(PRODUCER, encoding='utf-8')
        producer = root / 'Producer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>FieldThrowProducer</Name>'
            '<OutputType>' + ('SharedLibrary' if dynamic else 'StaticLibrary') + '</OutputType></PropertyGroup>'
            '<ItemGroup><Compile Include="Producer.ns"/>' + reference(core, core_metadata) +
            '</ItemGroup></Project>', encoding='utf-8')
        consumer = root / 'Consumer.nsproj'
        for producer_level in ('O0', 'O2'):
            library = root / ('producer-' + producer_level + '.lib')
            output = library.with_suffix('.dll') if dynamic else library
            result = run([compiler, '--emit-project-oop-dll' if dynamic else '--emit-project-oop-lib',
                          producer, '-' + producer_level, '-o', output, '--no-artifact-cache'], root)
            if result.returncode:
                failures.append((producer_level, 'producer'))
                continue
            consumer.write_text('<Project><PropertyGroup><Name>FieldThrowConsumer</Name></PropertyGroup>'
                '<ItemGroup><Compile Include="Main.ns"/>' + reference(core, core_metadata) +
                reference(library, library.with_suffix('.nsmeta')) + '</ItemGroup></Project>', encoding='utf-8')
            for name, operation in (('prefix-plus', '++holder.Item'), ('prefix-minus', '--holder.Item'),
                                    ('postfix-plus', 'holder.Item++'), ('postfix-minus', 'holder.Item--')):
                operation = operation.replace('holder.', RECEIVER + '.')
                (root / 'Main.ns').write_text(CONSUMER.replace('OPERATION', operation), encoding='utf-8')
                for level in ('O0', 'O2'):
                    executable = root / (producer_level + '-' + name + '-' + level + '.exe')
                    result = run([compiler, '--emit-project-oop-exe', consumer, '-' + level,
                                  '-o', executable, '--no-artifact-cache'], root)
                    if result.returncode:
                        failures.append((producer_level, name, level, 'compile'))
                    elif run([executable], root).returncode != 42:
                        failures.append((producer_level, name, level, 'execute'))
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
