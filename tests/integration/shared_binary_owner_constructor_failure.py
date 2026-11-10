"""Check shared-pointer field cleanup in a separately compiled constructor."""
from html import escape
from pathlib import Path
import subprocess
import sys
import tempfile


PRODUCER = '''using NativeSharp.Core;
public class Payload { public Payload() { } }
class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
public class Holder { public HANDLE<Payload> value; public int number;
 public void Initialize(HANDLE<Payload> input) { value = move(input); HELPER_FAILURE }
 public Holder(HANDLE<Payload> input) { Initialize(move(input)); CONSTRUCTOR_FAILURE } }
'''
CONSUMER = '''using NativeSharp.Core;
class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); int caught = 0;
 try { Holder holder = new Holder(INPUT); } catch (Exception error) { caught = 1; }
 return caught == 1 && owner.COUNT() == EXPECTED ? 42 : 20 + caught * 10 + (int)owner.COUNT();
} }
'''


def reference(library, metadata):
    return ('<LibraryReference Include="' + escape(library.as_posix(), quote=True) +
            '" Metadata="' + escape(metadata.as_posix(), quote=True) + '"/>')


def run(command, cwd):
    command = [str(item) for item in command]
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                            errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler, core, metadata = (Path(item).resolve() for item in sys.argv[1:4])
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-binary-owner-constructor-') as temporary:
        root = Path(temporary)
        producer = root / 'Producer.nsproj'
        consumer = root / 'Consumer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>SharedOwnerProducer</Name>'
            '<OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup><Compile Include="Producer.ns"/>' +
            reference(core, metadata) + '</ItemGroup></Project>', encoding='utf-8')
        for strong, weak, factory in (('Rc', 'WeakRc', 'MakeRc'), ('Arc', 'WeakArc', 'MakeArc')):
            for kind, handle, source, count, expected in (
                    ('strong', strong, 'owner.Clone()', 'StrongCount', '1l'),
                    ('weak', weak, 'owner.Downgrade()', 'WeakCount', '0l')):
                for where in ('constructor', 'helper'):
                    code = PRODUCER.replace('HANDLE', handle)
                    code = code.replace('HELPER_FAILURE', 'number = Gate.Read();' if where == 'helper' else '')
                    code = code.replace('CONSTRUCTOR_FAILURE', 'number = Gate.Read();' if where == 'constructor' else '')
                    (root / 'Producer.ns').write_text(code, encoding='utf-8')
                    code = CONSUMER.replace('STRONG', strong).replace('MAKE', factory)
                    code = code.replace('INPUT', source).replace('COUNT', count).replace('EXPECTED', expected)
                    (root / 'Main.ns').write_text(code, encoding='utf-8')
                    for producer_level in ('O0', 'O2'):
                        name = strong + '-' + kind + '-' + where + '-' + producer_level
                        library = root / (name + '.lib')
                        if run([compiler, '--emit-project-oop-lib', producer, '-' + producer_level,
                                '-o', library, '--no-artifact-cache'], root).returncode:
                            failures.append((name, 'producer'))
                            continue
                        consumer.write_text('<Project><PropertyGroup><Name>SharedOwnerConsumer</Name></PropertyGroup>'
                            '<ItemGroup><Compile Include="Main.ns"/>' + reference(core, metadata) +
                            reference(library, library.with_suffix('.nsmeta')) + '</ItemGroup></Project>',
                            encoding='utf-8')
                        for level in ('O0', 'O2'):
                            executable = root / (name + '-' + level + '.exe')
                            if run([compiler, '--emit-project-oop-exe', consumer, '-' + level,
                                    '-o', executable, '--no-artifact-cache'], root).returncode:
                                failures.append((name, level, 'compile'))
                                continue
                            result = run([executable], root)
                            if result.returncode != 42:
                                failures.append((name, level, 'execute', result.returncode))
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
