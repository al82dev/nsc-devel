"""A producer poll needs its error filter even when its source has no catch."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
from xml.sax.saxutils import quoteattr

PRODUCER = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public int Id;
public Exception(int id) { Id = id; } } }
public class Service { public static int Calls;
public static int Fail() throws Exception { Calls = Calls + 1; throw new Exception(7); }
public static async int Work() throws Exception { yield; return Fail(); } }
'''

CONSUMER = '''using NativeSharp.Core;
class Program { public static async int Main() { int caught = 0;
try { int ignored = await Service.Work(); return 1; } catch (Exception e) { caught = e.Id; }
return caught == 7 && Service.Calls == 1 ? 42 : 2; } }
'''


def invoke(command, cwd, expected):
    print('COMMAND', subprocess.list2cmdline(command), 'CWD', cwd, flush=True)
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=120)
    print('EXIT', result.returncode, result.stdout, result.stderr, flush=True)
    if result.returncode != expected:
        raise RuntimeError('unexpected exit ' + str(result.returncode))


def main():
    if os.name != 'nt' or len(sys.argv) != 2:
        return 77
    compiler = Path(sys.argv[1]).resolve()
    repository = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix='nsc-poll-external-') as temporary:
        root = Path(temporary)
        producer = root / 'producer'
        consumer = root / 'consumer'
        producer.mkdir()
        consumer.mkdir()
        (producer / 'Main.ns').write_text(PRODUCER, encoding='utf-8')
        (consumer / 'Main.ns').write_text(CONSUMER, encoding='utf-8')
        project = producer / 'Producer.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Producer</Name><OutputType>Library</OutputType>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>',
                           encoding='utf-8')
        for level in ('O0', 'O2'):
            library = producer / ('Producer-' + level + '.lib')
            invoke([str(compiler), '--emit-project-oop-lib', str(project), '-' + level,
                    '-o', str(library), '--no-artifact-cache'], repository, 0)
            metadata = library.with_suffix('.nsmeta')
            if not metadata.is_file():
                raise RuntimeError('missing producer metadata')
            consumer_project = consumer / 'Consumer.nsproj'
            consumer_project.write_text('<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup>'
                '<ItemGroup><Compile Include="Main.ns" /><LibraryReference Include=' + quoteattr(library.as_posix()) +
                ' Metadata=' + quoteattr(metadata.as_posix()) + ' /></ItemGroup></Project>', encoding='utf-8')
            executable = consumer / ('Consumer-' + level + '.exe')
            invoke([str(compiler), '--emit-project-oop-exe', str(consumer_project), '-' + level,
                    '-o', str(executable), '--no-artifact-cache'], repository, 0)
            invoke([str(executable)], repository, 42)
    return 0


if __name__ == '__main__':
    sys.exit(main())
