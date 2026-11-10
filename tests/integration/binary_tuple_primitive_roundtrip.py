from pathlib import Path
import subprocess
import sys

compiler = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
producer, consumer = root / 'producer', root / 'consumer'
producer.mkdir(parents=True, exist_ok=True)
consumer.mkdir(parents=True, exist_ok=True)

def run(args, cwd):
    command = [str(x) for x in args]
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                            errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode, flush=True)
    print(result.stdout + result.stderr, flush=True)
    return result.returncode

project = producer / 'Producer.nsproj'
project.write_text('<Project><PropertyGroup><Name>OnlyTupleProducer</Name>'
                   '<OutputType>Library</OutputType></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                   '</ItemGroup></Project>', encoding='utf-8')
(producer / 'Main.ns').write_text('''public class Reader {
 public static int ReadInts((int, int) value) { var (a, b) = value; return a + b; }
 public static int ReadNested(((int, int), int) value) { var ((a, b), c) = value; return a + b + c; }
 public static (int, int) MakePair() { return (40, 2); }
 public static int ReadMixed((int, bool) value) { var (number, enabled) = value; return enabled ? number + 2 : number; }
}''', encoding='utf-8')
consumer_project = consumer / 'Consumer.nsproj'
failures = []
for level in ('O0', 'O2'):
    library = producer / ('OnlyTupleProducer-' + level + '.lib')
    if run([compiler, '--emit-project-oop-lib', project, '-' + level, '-o', library,
            '--no-artifact-cache'], producer) != 0:
        failures.append(level + '-producer')
        continue
    consumer_project.write_text('<Project><PropertyGroup><Name>OnlyTupleConsumer</Name>'
        '</PropertyGroup><ItemGroup><Compile Include="Main.ns" /><LibraryReference Include="' +
        library.as_posix() + '" Metadata="' + library.with_suffix('.nsmeta').as_posix() +
        '" /></ItemGroup></Project>', encoding='utf-8')
    for name, body in [('plain', 'return Reader.ReadInts((40, 2));'),
                       ('nested', 'return Reader.ReadNested(((39, 2), 1));'),
                       ('return', 'var (a, b) = Reader.MakePair(); return a + b;'),
                       ('mixed', 'return Reader.ReadMixed((40, true));')]:
        (consumer / 'Main.ns').write_text('class Program { public static int Main() { '
                                          + body + ' } }', encoding='utf-8')
        exe = consumer / (name + '-' + level + '.exe')
        if run([compiler, '--emit-project-oop-exe', consumer_project, '-' + level, '-o', exe,
                '--no-artifact-cache'], consumer) != 0:
            failures.append(name + '-' + level + '-consumer')
        elif run([exe], consumer) != 42:
            failures.append(name + '-' + level + '-runtime')
print('FAILURES', failures)
sys.exit(bool(failures))
