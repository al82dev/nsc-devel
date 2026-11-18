"""Pending: conditional unsafe thread traits for a generic pointer wrapper."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                          errors='replace', timeout=120)


def main():
    compiler = Path(sys.argv[1]).resolve()
    options = sys.argv[2:]
    if any(option != '--split-impl' for option in options) or options.count('--split-impl') > 1:
        raise ValueError('expected only the optional --split-impl argument')
    split_impl = '--split-impl' in options
    declarations = '''public trait Send {} public trait Sync {}
namespace Impostor { public trait Send {} public trait Sync {} }
public struct Box<T> { public T* p; }
unsafe impl<T> Send for Box<T> where T : Send, Sync {}
unsafe impl<T> Sync for Box<T> where T : Send, Sync {}
public struct Unmarked<T> { public T* p; }
public struct Mismatched<T> { public T* p; }
unsafe impl<T> Send for Mismatched<T> where T : Impostor.Send, Impostor.Sync {}
unsafe impl<T> Sync for Mismatched<T> where T : Impostor.Send, Impostor.Sync {}
'''
    program = '''struct Safe { public int n; } struct Raw { public int* p; }
struct NestedRaw { public Raw value; }
struct SendOnly { public ref mut int p; }
struct SharedToSendOnly { public ref SendOnly value; }
struct ContainsBox { public Box<Safe> value; }
struct ContainsBadBox { public Box<Raw> value; }
class Program { static int Require<T>() where T : MARKER { return 42; }
 public static int Main() { return Require<TARGET>(); } }
'''
    cases = {'Box<Safe>': True, 'Box<int>': True, 'Box<Box<Safe>>': True,
             'Box<Box<Raw>>': False, 'ContainsBox': True, 'ContainsBadBox': False,
             'Box<SharedToSendOnly>': False,
             'Box<Raw>': False, 'Box<NestedRaw>': False,
             'Box<SendOnly>': False, 'Unmarked<Safe>': False, 'Mismatched<Safe>': False}
    if split_impl:
        declarations = declarations.replace(
            'public trait Send {} public trait Sync {}',
            'namespace Safety { public trait Send {} public trait Sync {} }', 1)
        program = 'using Safety;\n' + program
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-unsafe-thread-traits-') as temporary:
        root = Path(temporary)
        producer = root / 'producer'
        consumer = root / 'consumer'
        producer.mkdir()
        consumer.mkdir()
        if split_impl:
            lines = declarations.splitlines(keepends=True)
            implementations = [line for line in lines if line.startswith('unsafe impl')]
            assert len(implementations) == 4
            (producer / 'Types.ns').write_text(
                'using Safety;\n' + ''.join(line for line in lines if not line.startswith('unsafe impl')),
                encoding='utf-8')
            for index, implementation in enumerate(implementations):
                (producer / ('Impl' + str(index) + '.ns')).write_text(
                    'using Safety;\n' + implementation, encoding='utf-8')
        else:
            (producer / 'Types.ns').write_text(declarations, encoding='utf-8')
        producer_project = producer / 'Producer.nsproj'
        producer_project.write_text('<Project><PropertyGroup><Name>UnsafeProducer</Name>'
                                    '<OutputType>Library</OutputType>'
                                    '</PropertyGroup><ItemGroup><Compile Include="*.ns" />'
                                    '</ItemGroup></Project>', encoding='utf-8')
        project = consumer / 'Consumer.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Consumer</Name>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                           '<ProjectReference Include="../producer/Producer.nsproj" />'
                           '</ItemGroup></Project>', encoding='utf-8')
        for target, expected in cases.items():
            for marker in ('Send', 'Sync'):
                (consumer / 'Main.ns').write_text(program.replace('MARKER', marker).replace(
                    'TARGET', target), encoding='utf-8')
                result = run([str(compiler), '--check-project-method-bodies', str(project)], consumer)
                output = result.stdout + result.stderr
                accepted = result.returncode == 0
                correct = accepted == expected and (accepted or 'requires an implementation of' in output)
                print('source', marker, target, 'accepted=' + str(accepted),
                      'expected=' + str(expected), flush=True)
                if not correct:
                    failures.append(('source', marker, target, output))
        library = producer / 'Producer.lib'
        result = run([str(compiler), '--emit-project-oop-lib', str(producer_project),
                      '-O0', '-o', str(library), '--no-artifact-cache'], producer)
        print('binary producer exit=' + str(result.returncode), flush=True)
        if result.returncode != 0:
            failures.append(('binary producer', result.stdout + result.stderr))
        else:
            project.write_text('<Project><PropertyGroup><Name>Consumer</Name>'
                               '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                               '<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                               library.with_suffix('.nsmeta').as_posix() + '" />'
                               '</ItemGroup></Project>', encoding='utf-8')
            for target, expected in cases.items():
                for marker in ('Send', 'Sync'):
                    (consumer / 'Main.ns').write_text(program.replace('MARKER', marker).replace(
                        'TARGET', target), encoding='utf-8')
                    result = run([str(compiler), '--check-project-method-bodies', str(project)], consumer)
                    output = result.stdout + result.stderr
                    accepted = result.returncode == 0
                    correct = accepted == expected and (accepted or 'requires an implementation of' in output)
                    print('binary', marker, target, 'accepted=' + str(accepted),
                          'expected=' + str(expected), flush=True)
                    if not correct:
                        failures.append(('binary', marker, target, output))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
