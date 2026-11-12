"""Exercise the conservative binary boundary for tuples containing SharedView."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, cwd):
    command = [str(value) for value in command]
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                            errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def reference(library):
    return ('<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
            library.with_suffix('.nsmeta').as_posix() + '" />')


def field(data, cursor):
    colon = data.index(':', cursor)
    end = colon + 1 + int(data[cursor:colon])
    if data[end] != ';':
        raise ValueError('invalid API field')
    return data[colon + 1:end], end + 1


def items(data):
    cursor = 0
    while cursor < len(data):
        value, cursor = field(data, cursor)
        yield value


def escape_summaries(output):
    data = next(line for line in output.splitlines() if line.startswith('25:NativeSharp.source-api.v2;'))
    _, cursor = field(data, 0)
    count, cursor = field(data, cursor)
    result = {}
    for _ in range(int(count)):
        key, cursor = field(data, cursor)
        contract, cursor = field(data, cursor)
        if not key.startswith('member:type:ViewTupleProducer::Reader:2:'):
            continue
        parts = iter(items(contract))
        prefix = [next(parts) for _ in range(10)]
        parameter_count = int(next(parts))
        for _ in range(parameter_count * 14): next(parts)
        next(parts)
        for _ in range(int(next(parts))): next(parts)
        for _ in range(int(next(parts)) * 2): next(parts)
        result[prefix[3]] = [int(next(parts)) for _ in range(int(next(parts)))]
    return result


def main():
    compiler, core = (Path(value).resolve() for value in sys.argv[1:3])
    with tempfile.TemporaryDirectory(prefix='nsc-binary-view-tuple-') as directory:
        root = Path(directory)
        producer = root / 'producer'
        consumer = root / 'consumer'
        producer.mkdir()
        consumer.mkdir()
        producer_project = producer / 'Producer.nsproj'
        producer_project.write_text('<Project><PropertyGroup><Name>ViewTupleProducer</Name>'
            '<OutputType>Library</OutputType></PropertyGroup><ItemGroup><Compile Include="Main.ns" />' +
            reference(core) + '</ItemGroup></Project>', encoding='utf-8')
        source = producer / 'Main.ns'
        source.write_text('''using NativeSharp.Core;
public class Payload { public int Data; public Payload() { Data = 42; } }
public class Reader {
 public static int Read((SharedView<Payload>, int) pair) { var (v, n) = pair; return v.Get().Data + n; }
 public static int ReadForward((SharedView<Payload>, int) pair) { return Read(pair); }
 public static (SharedView<Payload>, int) Echo((SharedView<Payload>, int) pair) { return pair; }
 public static int ReadInts((int, int) pair) { var (a, b) = pair; return a + b; }
}''', encoding='utf-8')
        library = producer / 'ViewTupleProducer.lib'
        result = run([compiler, '--emit-project-oop-lib', producer_project, '-O0', '-o', library,
                      '--no-artifact-cache'], producer)
        if result.returncode:
            return 1
        surface = run([compiler, '--dump-api-surface', library.with_suffix('.nsmeta')], producer)
        expected = {'Read': [0], 'ReadForward': [0], 'Echo': [1], 'ReadInts': [0]}
        if surface.returncode or escape_summaries(surface.stdout) != expected:
            print('Unexpected published escape summaries', flush=True)
            return 1
        project = consumer / 'Consumer.nsproj'
        project.write_text('<Project><PropertyGroup><Name>ViewTupleConsumer</Name></PropertyGroup>'
            '<ItemGroup><Compile Include="Main.ns" />' + reference(core) + reference(library) +
            '</ItemGroup></Project>', encoding='utf-8')
        main_file = consumer / 'Main.ns'
        failures = []
        for family in ('Rc', 'Arc'):
            for form in ('direct', 'local'):
                declaration = '' if form == 'direct' else '(SharedView<Payload>, int) pair = (owner.Borrow(), 0);'
                argument = '(owner.Borrow(), 0)' if form == 'direct' else 'pair'
                for method in ('Read', 'ReadForward', 'Echo'):
                    call = ('Reader.Echo(' + argument + '); return 0;' if method == 'Echo'
                            else 'return Reader.' + method + '(' + argument + ');')
                    main_file.write_text('using NativeSharp.Core; class Program { public static int Main() { '
                        + family + '<Payload> owner = SharedPointers.Make' + family + '<Payload>(); '
                        + declaration + call + ' } }', encoding='utf-8')
                    result = run([compiler, '--check-project-method-bodies', project], consumer)
                    message = 'contains a borrowed view whose non-escaping contract cannot be verified'
                    if result.returncode == 0 or message not in result.stdout + result.stderr:
                        failures.append((family, form, method, 'expected conservative escape rejection'))
        main_file.write_text('class Program { public static int Main() { return Reader.ReadInts((40, 2)); } }',
                             encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = consumer / ('plain-' + level + '.exe')
            if run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', executable,
                    '--no-artifact-cache'], consumer).returncode or run([executable], consumer).returncode != 42:
                failures.append(('plain', level))
        counterfeit = root / 'counterfeit'
        counterfeit.mkdir()
        (counterfeit / 'Main.ns').write_text('''namespace NativeSharp.Core {
 public struct SharedView<T> { public int Number; } }
public class Payload { }
public class Reader { public static int Ping(int value) { return value; }
 public static int Read((NativeSharp.Core.SharedView<Payload>, int) pair) { var (v, n) = pair; return n; } }
''', encoding='utf-8')
        counterfeit_project = counterfeit / 'Producer.nsproj'
        counterfeit_project.write_text('<Project><PropertyGroup><Name>CounterfeitView</Name>'
            '<OutputType>Library</OutputType></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
            '</ItemGroup></Project>', encoding='utf-8')
        counterfeit_lib = counterfeit / 'CounterfeitView.lib'
        result = run([compiler, '--emit-project-oop-lib', counterfeit_project, '-O0', '-o', counterfeit_lib,
                      '--no-artifact-cache'], counterfeit)
        if result.returncode:
            failures.append(('counterfeit', 'producer'))
        else:
            project.write_text('<Project><PropertyGroup><Name>ViewTupleConsumer</Name></PropertyGroup>'
                '<ItemGroup><Compile Include="Main.ns" />' + reference(counterfeit_lib) +
                '</ItemGroup></Project>', encoding='utf-8')
            main_file.write_text('class Program { public static int Main() { return Reader.Ping(42); } }',
                                 encoding='utf-8')
            if run([compiler, '--check-project-method-bodies', project], consumer).returncode:
                failures.append(('counterfeit', 'control'))
            main_file.write_text('using NativeSharp.Core; class Program { public static int Main() { '
                'return Reader.Read((default(SharedView<Payload>), 42)); } }', encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', project], consumer)
            if result.returncode == 0 or 'no compatible member' not in result.stdout + result.stderr:
                failures.append(('counterfeit', 'unexpected identity handling'))
        source.write_text('''using NativeSharp.Core;
public class Payload { public int Data; }
public class Sink { public static (SharedView<Payload>, int) saved;
 public static void Save((SharedView<Payload>, int) pair) { saved = pair; } }
''', encoding='utf-8')
        result = run([compiler, '--emit-project-oop-lib', producer_project, '-O0', '-o', producer / 'Unsafe.lib',
                      '--no-artifact-cache'], producer)
        if result.returncode == 0 or 'tuple containing a borrowed view cannot be stored outside a tracked local' not in result.stdout + result.stderr:
            failures.append(('producer', 'unsafe global storage was not rejected'))
        print('FAILURES', failures, flush=True)
        return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
