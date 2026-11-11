import os
from pathlib import Path
import subprocess
import sys
import tempfile

if len(sys.argv) > 2:
    core_argument = sys.argv[2]
else:
    core_argument = os.environ.get('NSC_TEST_CORE_BINARY')
if not core_argument:
    print('SKIP: set NSC_TEST_CORE_BINARY to an isolated NativeSharp.Core library', flush=True)
    sys.exit(77)
compiler, core = map(lambda s: Path(s).resolve(), (sys.argv[1], core_argument))
failures = []
cases = {
    'weak': ('''using NativeSharp.Core;
namespace Fixture { public class Payload { public Payload() { } }
 public WeakArc<Payload> value; }''',
             '''value = owner.Downgrade(); if (owner.WeakCount() != 1l) return 1;
 value = default(WeakArc<Payload>);'''),
    'tuple': ('''using NativeSharp.Core; using Pair = (int, WeakArc<Fixture.Payload>); using Other = Pair;
namespace Fixture { public class Payload { public Payload() { } }
 public Other value; }''',
              '''value = (0, owner.Downgrade()); if (owner.WeakCount() != 1l) return 1;
 value = (0, default(WeakArc<Payload>));'''),
    'strong': ('''using NativeSharp.Core;
namespace Fixture { public class Payload { public Payload() { } }
 public Arc<Payload> value; }''',
               '''value = owner.Clone(); if (owner.StrongCount() != 2l) return 3;
 value = default(Arc<Payload>); if (owner.StrongCount() != 1l) return 4;'''),
    'strong_tuple': ('''using NativeSharp.Core; using Pair = (int, Arc<Fixture.Payload>); using Other = Pair;
namespace Fixture { public class Payload { public Payload() { } }
 public Other value; }''',
                     '''value = (0, owner.Clone()); if (owner.StrongCount() != 2l) return 3;
 value = (0, default(Arc<Payload>)); if (owner.StrongCount() != 1l) return 4;'''),
}

def run(command, root):
    command = [str(x) for x in command]
    result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result.returncode

def reference(path):
    return '<LibraryReference Include="' + path.as_posix() + '" Metadata="' + \
        path.with_suffix('.nsmeta').as_posix() + '"/>'

with tempfile.TemporaryDirectory(prefix='nsc-namespace-weakarc-binary-') as directory:
    root = Path(directory)
    producer = root / 'Producer.nsproj'
    producer.write_text('<Project><PropertyGroup><Name>Producer</Name><OutputType>StaticLibrary</OutputType>'
        '</PropertyGroup><ItemGroup><Compile Include="Producer.ns"/>' + reference(core) +
        '</ItemGroup></Project>', encoding='utf-8')
    consumer = root / 'Consumer.nsproj'
    for name, (source, body) in cases.items():
        (root / 'Producer.ns').write_text(source, encoding='utf-8')
        (root / 'Consumer.ns').write_text('using NativeSharp.Core; using Fixture; class Program { '
            'public static int Main() { Arc<Payload> owner = SharedPointers.MakeArc<Payload>(); ' +
            body + ' return owner.WeakCount() == 0l ? 42 : 2; } }', encoding='utf-8')
        for level in ('O0', 'O2'):
            library = root / (name + '-' + level + '.lib')
            if run([compiler, '--emit-project-oop-lib', producer, '-' + level, '-o', library,
                    '--no-artifact-cache'], root):
                failures.append((name, level, 'producer'))
                continue
            consumer.write_text('<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup>'
                '<ItemGroup><Compile Include="Consumer.ns"/>' + reference(core) + reference(library) +
                '</ItemGroup></Project>', encoding='utf-8')
            executable = root / (name + '-' + level + '.exe')
            if run([compiler, '--emit-project-oop-exe', consumer, '-' + level, '-o', executable,
                    '--no-artifact-cache'], root):
                failures.append((name, level, 'consumer'))
            elif run([executable], root) != 42:
                failures.append((name, level, 'execution'))
    producer.write_text('<Project><PropertyGroup><Name>ShadowCore</Name><OutputType>StaticLibrary</OutputType>'
        '</PropertyGroup><ItemGroup><Compile Include="Producer.ns"/></ItemGroup></Project>', encoding='utf-8')
    for family in ('WeakArc', 'Arc'):
        (root / 'Producer.ns').write_text('''namespace NativeSharp.Core {
 public struct FAMILY<T> { public int Number; } }
public class Payload { }
using Pair = (int, NativeSharp.Core.FAMILY<Payload>);
namespace Fixture { public Pair value; }'''.replace('FAMILY', family), encoding='utf-8')
        command = [str(compiler), '--emit-project-oop-lib', str(producer), '-O0', '-o',
                   str(root / ('shadow-' + family + '.lib')), '--no-artifact-cache']
        result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
        output = result.stdout + result.stderr
        print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode, output, flush=True)
        if result.returncode == 0 or 'metadata-backed tuple source import cannot yet materialize element' not in output:
            failures.append(('counterfeit', family, 'unexpected identity handling'))
print('FAILURES', failures, flush=True)
sys.exit(bool(failures))
