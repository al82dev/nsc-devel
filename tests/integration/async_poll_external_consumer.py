"""A producer poll needs its error filter even when its source has no catch."""

import os
from contextlib import nullcontext
from pathlib import Path
import subprocess
import sys
from isolated_a_runner import absolute_a, setup
from xml.sax.saxutils import quoteattr

PRODUCER = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public int Id;
public Exception(int id) { Id = id; } } }
public class Service { public static int Calls;
public static int Fail() throws Exception { Calls = Calls + 1; throw new Exception(7); }
public static async int Work() throws Exception { yield; return Fail(); }
public static Future<int, Exception> Relay() { return Work(); }
public static async int Plain() { yield; return 5; }
public static Future<int> Make(bool fail) throws Exception {
 if (fail) throw new Exception(9); return Plain(); } }
'''

CONSUMER = '''using NativeSharp.Core;
class Program { public static async int Main() { int caught = 0;
int normal = 0; try { normal = await Service.Make(false); } catch (Exception e) { return 6; }
if (normal != 5) return 3;
try { int x = await Service.Make(true); return 4; }
catch (Exception e) { if (e.Id != 9) return 5; }
try { int ignored = await Service.Work(); return 1; } catch (Exception e) { caught = e.Id; }
try { int ignored = await Service.Relay(); return 7; } catch (Exception e) { caught = caught + e.Id; }
return caught == 14 && Service.Calls == 2 ? 42 : 2; } }
'''


def invoke(command, cwd, expected):
    print('COMMAND', subprocess.list2cmdline(command), 'CWD', cwd, flush=True)
    try:
        result = subprocess.run(command, cwd=cwd, env=ENVIRONMENT, capture_output=True, text=True,
                                errors='replace', timeout=10 if expected == 42 else 120)
    except subprocess.TimeoutExpired as error:
        raise RuntimeError('timed out: ' + subprocess.list2cmdline(command)) from error
    print('EXIT', result.returncode, result.stdout, result.stderr, flush=True)
    if result.returncode != expected:
        raise RuntimeError('unexpected exit ' + str(result.returncode))


def main():
    if os.name != 'nt' or len(sys.argv) != 3:
        return 77
    compiler = absolute_a(sys.argv[1])
    evidence = absolute_a(sys.argv[2])
    repository = Path(__file__).resolve().parents[2]
    global ENVIRONMENT
    ENVIRONMENT = setup(evidence, repository)
    with nullcontext(evidence / 'cases') as root:
        root.mkdir()
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
            for consumer_level in ('O0', 'O2'):
                executable = consumer / ('Consumer-' + level + '-' + consumer_level + '.exe')
                invoke([str(compiler), '--emit-project-oop-exe', str(consumer_project), '-' + consumer_level,
                        '-o', str(executable), '--no-artifact-cache'], repository, 0)
                invoke([str(executable)], repository, 42)
    return 0


if __name__ == '__main__':
    sys.exit(main())
