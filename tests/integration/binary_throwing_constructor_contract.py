"""Check NativeSharp binary constructors preserve their declared error contract."""
from html import escape
import hashlib
from pathlib import Path
import re
import subprocess
import sys

from isolated_a_runner import absolute_a, setup

REPOSITORY = Path(__file__).resolve().parents[2]
PRODUCER = '''using NativeSharp.Core;
public class Guard { public static int Drops; ~Guard() { Drops = Drops + 1; } }
public class Throwing {
 public static int Drops; public owned Guard item;
 public Throwing() throws Exception { item = new Guard(); throw new Exception(); }
 ~Throwing() { Drops = Drops + 1; }
}
public class Safe { public static int Drops; public Safe() { } ~Safe() { Drops = Drops + 1; } }
'''
CONSUMER = '''using NativeSharp.Core;
class Program { public static int Main() {
 int caught = 0;
 try { owned Throwing value = new Throwing(); } catch (Exception error) { caught = caught + 1; }
 { owned Safe value = new Safe(); }
 return caught == 1 && Guard.Drops == 1 && Throwing.Drops == 0 && Safe.Drops == 1 ? 42 : 1;
} }
'''

def reference(library, metadata):
    return ('<LibraryReference Include="' + escape(library.as_posix(), quote=True) +
            '" Metadata="' + escape(metadata.as_posix(), quote=True) + '"/>')

def main():
    if len(sys.argv) != 5:
        raise SystemExit('expected nsc, Core.lib, Core.nsmeta and new evidence directory')
    compiler, core, metadata, root = (absolute_a(value) for value in sys.argv[1:])
    env = setup(root, REPOSITORY)
    env['NS_LIB_ROOT'] = str(core.parent)
    before = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in (compiler, core, metadata)}
    records = []
    for producer_level in ('O0', 'O2'):
        folder = root / producer_level
        folder.mkdir()
        (folder / 'Producer.ns').write_text(PRODUCER, encoding='utf-8')
        producer = folder / 'Producer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>ConstructorContract</Name>'
            '<OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup>'
            '<Compile Include="Producer.ns"/>' + reference(core, metadata) + '</ItemGroup></Project>', encoding='utf-8')
        library = folder / 'Producer.lib'
        commands = [[compiler, '--emit-project-oop-lib', producer, '-' + producer_level,
                     '-o', library, '--no-artifact-cache', '--no-error-log']]
        consumer = folder / 'Consumer.nsproj'
        (folder / 'Consumer.ns').write_text(CONSUMER, encoding='utf-8')
        consumer.write_text('<Project><PropertyGroup><Name>ConstructorConsumer</Name></PropertyGroup><ItemGroup>'
            '<Compile Include="Consumer.ns"/>' + reference(core, metadata) +
            reference(library, library.with_suffix('.nsmeta')) + '</ItemGroup></Project>', encoding='utf-8')
        for command in commands:
            result = subprocess.run([str(x) for x in command], cwd=REPOSITORY, env=env,
                                    capture_output=True, text=True, errors='replace', timeout=120)
            records.append((str(command), result.returncode, result.stdout, result.stderr))
            if result.returncode:
                print(records[-1], flush=True)
                return 1
        command = [compiler, '--dump-project-oop-mir', consumer]
        inspected = subprocess.run([str(x) for x in command], cwd=REPOSITORY, env=env,
                                   capture_output=True, text=True, errors='replace', timeout=120)
        records.append((str(command), inspected.returncode, inspected.stdout, inspected.stderr))
        constructor_lines = [line for line in inspected.stdout.splitlines() if line.startswith('function #')]
        throwing = [line for line in constructor_lines if re.search(r'Throwing__Throwing\b', line)]
        safe = [line for line in constructor_lines if re.search(r'Safe__Safe\b', line)]
        if inspected.returncode or len(throwing) != 1 or 'throws=type:NativeSharpCore::NativeSharp.Core.Exception' not in throwing[0] or len(safe) != 1 or 'throws=<none>' not in safe[0]:
            print('unexpected imported constructor contracts', throwing, safe, inspected.stderr, flush=True)
            return 1
        for level in ('O0', 'O2'):
            executable = folder / (level + '.exe')
            command = [compiler, '--emit-project-oop-exe', consumer, '-' + level,
                       '-o', executable, '--no-artifact-cache', '--no-error-log']
            result = subprocess.run([str(x) for x in command], cwd=REPOSITORY, env=env,
                                    capture_output=True, text=True, errors='replace', timeout=120)
            records.append((str(command), result.returncode, result.stdout, result.stderr))
            if result.returncode:
                print(records[-1], flush=True)
                return 1
            executed = subprocess.run([str(executable)], cwd=REPOSITORY, env=env,
                                      capture_output=True, text=True, errors='replace', timeout=10)
            records.append((str(executable), executed.returncode, executed.stdout, executed.stderr))
            print(producer_level, level, 'run', executed.returncode, flush=True)
            if executed.returncode != 42:
                print(records[-1], flush=True)
                return 1
    after = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in (compiler, core, metadata)}
    (root / 'results.txt').write_text(repr({'before': before, 'after': after, 'records': records}), encoding='utf-8')
    return int(before != after)

if __name__ == '__main__':
    sys.exit(main())
