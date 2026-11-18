from pathlib import Path
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()
failures = []
sources = {
    'scalar': ('''namespace Fixture { int value; public int ReadWrite() {
 value = 42; return value; } }''', 'return ReadWrite();'),
    'chained_tuple': ('''using Pair = (int, long); using Other = Pair;
namespace Fixture { Other value; public int ReadWrite() {
 value = (40, 2l); var (first, second) = value; return first + (int)second; } }''', 'return ReadWrite();'),
    'direct_scalar': ('namespace Fixture { public int value; }', 'value = 42; return value;'),
    'direct_chained_tuple': ('''using Pair = (int, long); using Other = Pair;
namespace Fixture { public Other value; }''',
        'value = (40, 2l); var (first, second) = value; return first + (int)second;'),
}

def run(command, root):
    result = subprocess.run([str(x) for x in command], cwd=root, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline([str(x) for x in command]), 'EXIT',
          result.returncode, result.stdout + result.stderr, flush=True)
    return result.returncode

with tempfile.TemporaryDirectory(prefix='nsc-namespace-tuple-binary-') as directory:
    root = Path(directory)
    producer = root / 'Producer.nsproj'
    producer.write_text('<Project><PropertyGroup><Name>Producer</Name><OutputType>StaticLibrary</OutputType>'
                        '</PropertyGroup><ItemGroup><Compile Include="Producer.ns"/></ItemGroup></Project>',
                        encoding='utf-8')
    consumer = root / 'Consumer.nsproj'
    for name, (source, body) in sources.items():
        (root / 'Producer.ns').write_text(source, encoding='utf-8')
        (root / 'Consumer.ns').write_text('using Fixture; class Program { public static int Main() { '
                                          + body + ' } }', encoding='utf-8')
        for level in ('O0', 'O2'):
            library = root / (name + '-' + level + '.lib')
            if run([compiler, '--emit-project-oop-lib', producer, '-' + level, '-o', library,
                    '--no-artifact-cache'], root):
                failures.append((name, level, 'producer'))
                continue
            consumer.write_text('<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup>'
                '<ItemGroup><Compile Include="Consumer.ns"/><LibraryReference Include="' +
                library.as_posix() + '" Metadata="' + library.with_suffix('.nsmeta').as_posix() +
                '"/></ItemGroup></Project>', encoding='utf-8')
            executable = root / (name + '-' + level + '.exe')
            if run([compiler, '--emit-project-oop-exe', consumer, '-' + level, '-o', executable,
                    '--no-artifact-cache'], root):
                failures.append((name, level, 'consumer'))
            elif run([executable], root) != 42:
                failures.append((name, level, 'execution'))
    (root / 'Producer.ns').write_text('namespace Fixture { public (int, +) value; }', encoding='utf-8')
    command = [str(compiler), '--emit-project-oop-lib', str(producer), '-O0', '-o',
               str(root / 'invalid.lib'), '--no-artifact-cache']
    result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
    output = result.stdout + result.stderr
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode, output, flush=True)
    if result.returncode == 0 or 'expected a global declaration' not in output:
        failures.append(('invalid_tuple', 'O0', 'missing parser rejection'))
print('FAILURES', failures, flush=True)
sys.exit(bool(failures))
