"""Verify view outputs of methods on receivers loaded from a binary library."""
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
    result = subprocess.run([str(item) for item in command], cwd=cwd, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline([str(item) for item in command]), 'EXIT',
          result.returncode, result.stdout + result.stderr, flush=True)
    return result

with tempfile.TemporaryDirectory(prefix='nsc-binary-view-receiver-') as directory:
    root = Path(directory)
    producer = root / 'producer'
    consumer = root / 'consumer'
    producer.mkdir()
    consumer.mkdir()
    source = 'using NativeSharp.Core; public class Payload { public int Data; public Payload() { Data = 42; } } '
    for family in ('Rc', 'Arc'):
        source += ('public class Holder' + family + ' { private ' + family + '<Payload> inner; '
                   'public Holder' + family + '() { inner = SharedPointers.Make' + family + '<Payload>(); } '
                   'public void Reset() { inner = SharedPointers.Make' + family + '<Payload>(); } '
                   'public void SaveRef(ref SharedView<Payload> output) { output = inner.Borrow(); } '
                   'public void SaveOut(out SharedView<Payload> output) { output = inner.Borrow(); } } ')
    (producer / 'Main.ns').write_text(source, encoding='utf-8')
    producer_project = producer / 'Producer.nsproj'
    producer_project.write_text('<Project><PropertyGroup><Name>ReceiverProducer</Name><OutputType>Library</OutputType>'
        '</PropertyGroup><ItemGroup><Compile Include="Main.ns"/>' + reference(core) + '</ItemGroup></Project>', encoding='utf-8')
    library = producer / 'ReceiverProducer.lib'
    if run([compiler, '--emit-project-oop-lib', producer_project, '-O0', '-o', library,
            '--no-artifact-cache'], producer).returncode:
        print('FAILURES producer build', flush=True)
        sys.exit(1)
    project = consumer / 'Consumer.nsproj'
    project.write_text('<Project><PropertyGroup><Name>ReceiverConsumer</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns"/>' + reference(core) + reference(library) + '</ItemGroup></Project>', encoding='utf-8')
    failures = []
    for family in ('Rc', 'Arc'):
        for mode, method in (('ref', 'SaveRef'), ('out', 'SaveOut')):
            for variant in ('escape', 'replace', 'live', 'after', 'mutate', 'after_mutate'):
                declaration = 'SharedView<Payload> result = default(SharedView<Payload>); '
                holder = 'raii Holder' + family + ' owner = new Holder' + family + '(); '
                replacement = 'raii Holder' + family + ' next = new Holder' + family + '(); owner = move(next); '
                call = 'owner.' + method + '(' + mode + ' result); '
                body = (declaration + '{ ' + holder + call + '} return result.Get().Data;' if variant == 'escape' else
                        holder + '{ ' + declaration + call + 'int value = result.Get().Data; } ' + replacement +
                        'return 42;' if variant == 'after' else
                        holder + '{ ' + declaration + call + 'int value = result.Get().Data; } owner.Reset(); return 42;' if variant == 'after_mutate' else
                        holder + declaration + call + (replacement if variant == 'replace' else 'owner.Reset(); ' if variant == 'mutate' else '') + 'return result.Get().Data;')
                source = 'using NativeSharp.Core; class Program { public static int Main() { ' + body + ' } }'
                (consumer / 'Main.ns').write_text(source, encoding='utf-8')
                print('CASE', family, mode, variant, '\nSOURCE', source, flush=True)
                result = run([compiler, '--check-project-method-bodies', project], consumer)
                if (result.returncode == 0) != (variant in ('live', 'after', 'after_mutate')):
                    failures.append((family, mode, variant, result.returncode))
                elif result.returncode and not any(message in result.stdout + result.stderr for message in
                    ('view output cannot outlive its possible owner', 'while a borrowed local is alive',
                     'cannot call potentially mutating method while a borrowed field view is alive')):
                    failures.append((family, mode, variant, 'unexpected diagnostic'))
                if variant in ('live', 'after', 'after_mutate') and result.returncode == 0:
                    for level in ('O0', 'O2'):
                        executable = consumer / (family + '-' + mode + '-' + variant + '-' + level + '.exe')
                        if run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', executable,
                                '--no-artifact-cache'], consumer).returncode:
                            failures.append((family, mode, variant, level, 'build'))
                        elif run([executable], consumer).returncode != 42:
                            failures.append((family, mode, variant, level, 'execution'))
    print('FAILURES', failures, flush=True)
    sys.exit(bool(failures))
