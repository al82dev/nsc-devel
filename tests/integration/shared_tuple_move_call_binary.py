import os
from pathlib import Path
import subprocess
import sys
import tempfile

core_argument = sys.argv[2] if len(sys.argv) > 2 else os.environ.get('NSC_TEST_CORE_BINARY')
if not core_argument:
    print('SKIP: set NSC_TEST_CORE_BINARY to an isolated NativeSharp.Core library', flush=True)
    sys.exit(77)
compiler, core = (Path(value).resolve() for value in (sys.argv[1], core_argument))
failures = []

def reference(path):
    return '<LibraryReference Include="' + path.as_posix() + '" Metadata="' + \
        path.with_suffix('.nsmeta').as_posix() + '"/>'

def run(command, root):
    command = [str(value) for value in command]
    result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result.returncode

with tempfile.TemporaryDirectory(prefix='nsc-tuple-move-call-binary-') as directory:
    root = Path(directory)
    producer = root / 'Producer.nsproj'
    consumer = root / 'Consumer.nsproj'
    producer.write_text('<Project><PropertyGroup><Name>Producer</Name><OutputType>StaticLibrary</OutputType>'
        '</PropertyGroup><ItemGroup><Compile Include="Producer.ns"/>' + reference(core) +
        '</ItemGroup></Project>', encoding='utf-8')
    for strong, weak in (('Rc', 'WeakRc'), ('Arc', 'WeakArc')):
        for kind, handle in (('strong', strong), ('weak', weak)):
            for throws in (False, True):
                action = 'throw new Exception();' if throws else ''
                declaration = ' throws Exception' if throws else ''
                producer_source = ('using NativeSharp.Core; namespace Fixture { public class Payload { '
                    'public Payload() { } } public class Box { public static void Consume((' + handle +
                    '<Payload>, int) pair)' + declaration + ' { ' + action + ' } } }')
                before = (weak + '<Payload> observer = owner.Downgrade(); '
                          if kind == 'strong' else weak + '<Payload> value = owner.Downgrade(); ')
                operand = 'owner' if kind == 'strong' else 'value'
                verify = 'observer.Expired()' if kind == 'strong' else 'owner.WeakCount() == 0l'
                call = 'Box.Consume((move(' + operand + '), 0));'
                if throws:
                    call = 'int caught = 0; try { ' + call + ' } catch (Exception e) { caught = 1; } '
                    verify = 'caught == 1 && ' + verify
                consumer_source = ('using NativeSharp.Core; using Fixture; class Program { '
                    'public static int Main() { ' + strong + '<Payload> owner = SharedPointers.Make' +
                    strong + '<Payload>(); ' + before + call + ' return ' + verify + ' ? 42 : 1; } }')
                (root / 'Producer.ns').write_text(producer_source, encoding='utf-8')
                (root / 'Consumer.ns').write_text(consumer_source, encoding='utf-8')
                print('CASE', handle, 'throws', throws, '\nPRODUCER', producer_source,
                      '\nCONSUMER', consumer_source, flush=True)
                for level in ('O0', 'O2'):
                    stem = handle + '-' + str(throws) + '-' + level
                    library = root / (stem + '.lib')
                    if run([compiler, '--emit-project-oop-lib', producer, '-' + level, '-o', library,
                            '--no-artifact-cache'], root):
                        failures.append((stem, 'producer'))
                        continue
                    consumer.write_text('<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup>'
                        '<ItemGroup><Compile Include="Consumer.ns"/>' + reference(core) + reference(library) +
                        '</ItemGroup></Project>', encoding='utf-8')
                    exe = root / (stem + '.exe')
                    if run([compiler, '--emit-project-oop-exe', consumer, '-' + level, '-o', exe,
                            '--no-artifact-cache'], root):
                        failures.append((stem, 'consumer'))
                    elif run([exe], root) != 42:
                        failures.append((stem, 'execution'))
print('FAILURES', failures, flush=True)
sys.exit(bool(failures))
