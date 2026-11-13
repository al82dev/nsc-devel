"""Exercise delegated value-field cleanup in a separately compiled producer."""

from html import escape
from pathlib import Path
import subprocess
import sys

from delegated_value_field_proven import TWO_FIELDS
from isolated_a_runner import absolute_a, setup


def main():
    if len(sys.argv) != 4:
        raise SystemExit('expected nsc, Core.lib, and a new evidence directory on A:')
    compiler, core, root = (absolute_a(value) for value in sys.argv[1:])
    metadata = core.with_suffix('.nsmeta')
    if not core.is_file() or not metadata.is_file():
        raise SystemExit('Core library and metadata must exist')
    repository = Path(__file__).resolve().parents[2]
    environment = setup(root, repository)
    core_ref = '<LibraryReference Include="' + escape(core.as_posix(), quote=True) + '" Metadata="' + \
        escape(metadata.as_posix(), quote=True) + '" />'
    failures = []
    for kind, declaration in (('class', 'owned Bad value = new Bad();'),
                              ('struct', 'Bad value = new Bad();')):
        case = root / kind
        case.mkdir()
        source = TWO_FIELDS.replace('namespace NativeSharp.Core { public class Exception {} }\n', '')
        values = {'KIND': kind, 'MODE': '1', 'DECLARATION': declaration, 'CAUGHT': '1',
                  'TRACE': '21', 'DROPS': '0', 'THINGS': '2',
                  'FIELD': 'public owned Thing Item;', 'INIT': 'Item = new Thing();'}
        for key, value in values.items():
            source = source.replace(key, value)
        source = source.replace('class Program { public static int Main()', 'public class ProbeApi { public static int Run()')
        (case / 'Producer.ns').write_text(source, encoding='utf-8')
        producer = case / 'Producer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>Producer</Name><OutputType>StaticLibrary</OutputType>'
                            '</PropertyGroup><ItemGroup><Compile Include="Producer.ns" />' + core_ref +
                            '</ItemGroup></Project>', encoding='utf-8')
        (case / 'Consumer.ns').write_text('class Program { public static int Main() { return ProbeApi.Run(); } }',
                                         encoding='utf-8')
        for producer_level in ('O0', 'O2'):
            library = case / ('Producer-' + producer_level + '.lib')
            command = [str(compiler), '--emit-project-oop-lib', str(producer), '-' + producer_level,
                       '-o', str(library), '--no-artifact-cache']
            print('COMMAND', subprocess.list2cmdline(command), flush=True)
            result = subprocess.run(command, cwd=repository, env=environment, capture_output=True,
                                    text=True, errors='replace', timeout=120)
            if result.returncode:
                failures.append((kind, producer_level, 'producer', result.stdout + result.stderr))
                continue
            producer_ref = '<LibraryReference Include="' + escape(library.as_posix(), quote=True) + \
                '" Metadata="' + escape(library.with_suffix('.nsmeta').as_posix(), quote=True) + '" />'
            consumer = case / ('Consumer-' + producer_level + '.nsproj')
            consumer.write_text('<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup><ItemGroup>'
                                '<Compile Include="Consumer.ns" />' + core_ref + producer_ref +
                                '</ItemGroup></Project>', encoding='utf-8')
            for consumer_level in ('O0', 'O2'):
                exe = case / (producer_level + '-' + consumer_level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(consumer), '-' + consumer_level,
                           '-o', str(exe), '--no-artifact-cache']
                print('COMMAND', subprocess.list2cmdline(command), flush=True)
                result = subprocess.run(command, cwd=repository, env=environment, capture_output=True,
                                        text=True, errors='replace', timeout=120)
                if result.returncode:
                    failures.append((kind, producer_level, consumer_level, 'consumer', result.stdout + result.stderr))
                    continue
                result = subprocess.run([str(exe)], cwd=repository, env=environment, capture_output=True,
                                        text=True, errors='replace', timeout=10)
                print('RUN', kind, producer_level, consumer_level, result.returncode, flush=True)
                if result.returncode != 42:
                    failures.append((kind, producer_level, consumer_level, 'run', result.returncode))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
