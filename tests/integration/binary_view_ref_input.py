"""Check owner provenance when a compiled library writes a view output."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()
core = Path(os.environ['NSC_TEST_CORE_BINARY']).resolve()

def reference(library):
    return ('<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
            library.with_suffix('.nsmeta').as_posix() + '"/>')

def run(command, cwd):
    result = subprocess.run([str(value) for value in command], cwd=cwd, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline([str(value) for value in command]),
          'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
    return result

with tempfile.TemporaryDirectory(prefix='nsc-binary-view-output-') as directory:
    root = Path(directory)
    producer = root / 'producer'
    consumer = root / 'consumer'
    producer.mkdir()
    consumer.mkdir()
    (producer / 'Main.ns').write_text('''using NativeSharp.Core;
public class Payload { public int Data; public Payload() { Data = 42; } }
public class Sink {
 public static void Save(ref SharedView<Payload> value, ref SharedView<Payload> output) { output = value; }
 public static void SaveOut(ref SharedView<Payload> value, out SharedView<Payload> output) { output = value; }
}''', encoding='utf-8')
    producer_project = producer / 'Producer.nsproj'
    producer_project.write_text('<Project><PropertyGroup><Name>OutputProducer</Name><OutputType>Library</OutputType>'
        '</PropertyGroup><ItemGroup><Compile Include="Main.ns"/>' + reference(core) + '</ItemGroup></Project>',
        encoding='utf-8')
    library = producer / 'OutputProducer.lib'
    result = run([compiler, '--emit-project-oop-lib', producer_project, '-O0', '-o', library,
                  '--no-artifact-cache'], producer)
    if result.returncode:
        print('FAILURES producer build', flush=True)
        sys.exit(1)
    project = consumer / 'Consumer.nsproj'
    project.write_text('<Project><PropertyGroup><Name>OutputConsumer</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns"/>' + reference(core) + reference(library) + '</ItemGroup></Project>',
        encoding='utf-8')
    failures = []
    for family in ('Rc', 'Arc'):
        for mode, method in (('ref', 'Save'), ('out', 'SaveOut')):
            for variant in ('escape', 'replace', 'live'):
                owner = family + '<Payload> owner = SharedPointers.Make' + family + '<Payload>(); '
                declaration = 'SharedView<Payload> result = default(SharedView<Payload>); '
                call = 'SharedView<Payload> source = owner.Borrow(); Sink.' + method + '(ref source, ' + mode + ' result); '
                replacement = 'owner = SharedPointers.Make' + family + '<Payload>(); '
                body = (declaration + '{ ' + owner + call + '} return result.Get().Data;' if variant == 'escape' else
                        owner + declaration + call + (replacement if variant == 'replace' else '') +
                        'return result.Get().Data;')
                source = 'using NativeSharp.Core; class Program { public static int Main() { ' + body + ' } }'
                (consumer / 'Main.ns').write_text(source, encoding='utf-8')
                print('CASE', family, mode, variant, '\nSOURCE', source, flush=True)
                result = run([compiler, '--check-project-method-bodies', project], consumer)
                if (result.returncode == 0) != (variant == 'live'):
                    failures.append((family, mode, variant, result.returncode))
                elif result.returncode and not any(message in result.stdout + result.stderr for message in
                    ('view output cannot outlive its possible owner', 'while a borrowed local is alive')):
                    failures.append((family, mode, variant, 'unexpected diagnostic'))
                if variant == 'live' and result.returncode == 0:
                    for level in ('O0', 'O2'):
                        executable = consumer / (family + '-' + mode + '-' + level + '.exe')
                        built = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', executable,
                                     '--no-artifact-cache'], consumer)
                        if built.returncode:
                            failures.append((family, mode, level, 'build'))
                        elif run([executable], consumer).returncode != 42:
                            failures.append((family, mode, level, 'execution'))
    print('FAILURES', failures, flush=True)
    sys.exit(bool(failures))
