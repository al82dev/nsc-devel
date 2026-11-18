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

def run(command, root):
    command = [str(value) for value in command]
    result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result.returncode

def reference(path):
    return '<LibraryReference Include="' + path.as_posix() + '" Metadata="' + \
        path.with_suffix('.nsmeta').as_posix() + '"/>'

with tempfile.TemporaryDirectory(prefix='nsc-shared-tuple-move-partial-') as directory:
    root = Path(directory)
    producer = root / 'Producer.nsproj'
    consumer = root / 'Consumer.nsproj'
    producer.write_text('<Project><PropertyGroup><Name>Producer</Name><OutputType>StaticLibrary</OutputType>'
        '</PropertyGroup><ItemGroup><Compile Include="Producer.ns"/>' + reference(core) +
        '</ItemGroup></Project>', encoding='utf-8')
    for strong, weak in (('Rc', 'WeakRc'), ('Arc', 'WeakArc')):
        for handle, source, count, expected in ((strong, 'owner.Clone()', 'StrongCount', '1l'),
                                                 (weak, 'owner.Downgrade()', 'WeakCount', '0l')):
            (root / 'Producer.ns').write_text('using NativeSharp.Core; namespace Fixture { '
                'public class Payload { public Payload() { } } public class Box { '
                'public static int Fail() throws Exception { throw new Exception(); } public static (' +
                handle + '<Payload>, int) Pack(' + handle + '<Payload> input) throws Exception { '
                'return (move(input), Fail()); } } }', encoding='utf-8')
            (root / 'Consumer.ns').write_text('using NativeSharp.Core; using Fixture; '
                'class Program { public static int Main() { ' + strong + '<Payload> owner = '
                'SharedPointers.Make' + strong + '<Payload>(); int caught = 0; try { (' + handle +
                '<Payload>, int) pair = Box.Pack(' + source + '); } catch (Exception e) { caught = 1; } '
                'return caught == 1 && owner.' + count + '() == ' + expected + ' ? 42 : 2; } }', encoding='utf-8')
            for level in ('O0', 'O2'):
                library = root / (handle + '-' + level + '.lib')
                if run([compiler, '--emit-project-oop-lib', producer, '-' + level, '-o', library,
                        '--no-artifact-cache'], root):
                    failures.append((handle, level, 'producer'))
                    continue
                consumer.write_text('<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup>'
                    '<ItemGroup><Compile Include="Consumer.ns"/>' + reference(core) + reference(library) +
                    '</ItemGroup></Project>', encoding='utf-8')
                executable = root / (handle + '-' + level + '.exe')
                if run([compiler, '--emit-project-oop-exe', consumer, '-' + level, '-o', executable,
                        '--no-artifact-cache'], root):
                    failures.append((handle, level, 'consumer'))
                elif run([executable], root) != 42:
                    failures.append((handle, level, 'execution'))
print('FAILURES', failures, flush=True)
sys.exit(bool(failures))
