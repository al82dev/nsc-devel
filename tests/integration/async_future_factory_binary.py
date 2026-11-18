"""Await static and virtual future factories from a separately compiled library."""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile
from xml.sax.saxutils import quoteattr


PRODUCER = '''namespace FactoryBinary {
 public class Base { public Base() { } public virtual Future<int> Make() { return Provider.Slow(10); } }
 public class Derived : Base { public Derived() { } public override Future<int> Make() { return Provider.Slow(40); } }
 public class Provider { public static async int Slow(int value) { yield; return value; }
  public static Future<int> Make() { return Slow(41); } }
}'''
CONSUMER = '''using FactoryBinary;
class Program { public static async int Main() {
 int first = await Provider.Make(); Base item = new Derived();
 int second = await item.Make(); return first == 41 && second == 40 ? 42 : 1;
} }'''


def run(command, repo):
    args = [str(item) for item in command]
    result = subprocess.run(args, cwd=repo, capture_output=True, text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(args), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def main():
    if os.name != 'nt':
        return 77
    compiler = Path(sys.argv[1]).resolve()
    repo = Path(__file__).resolve().parents[2]
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-future-factory-binary-') as temporary:
        root = Path(temporary)
        (root / 'Producer.ns').write_text(PRODUCER, encoding='utf-8')
        (root / 'Main.ns').write_text(CONSUMER, encoding='utf-8')
        producer = root / 'Producer.nsproj'
        consumer = root / 'Consumer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>FactoryBinary</Name>'
                            '<OutputType>StaticLibrary</OutputType></PropertyGroup>'
                            '<ItemGroup><Compile Include="Producer.ns" /></ItemGroup></Project>', encoding='utf-8')
        for level in ('O0', 'O2'):
            library = root / ('Producer-' + level + '.lib')
            cache = '--artifact-cache=' + str(root / ('cache-' + level))
            result = run([compiler, '--emit-project-oop-lib', producer, '-' + level, '-o', library, cache], repo)
            if result.returncode:
                failures.append((level, 'producer', result.returncode))
                continue
            metadata = library.with_suffix('.nsmeta')
            if not metadata.is_file():
                failures.append((level, 'metadata missing'))
                continue
            consumer.write_text('<Project><PropertyGroup><Name>FactoryConsumer</Name></PropertyGroup>'
                                '<ItemGroup><Compile Include="Main.ns" /><LibraryReference Include=' +
                                quoteattr(library.as_posix()) + ' Metadata=' + quoteattr(metadata.as_posix()) +
                                ' /></ItemGroup></Project>', encoding='utf-8')
            executable = root / ('Consumer-' + level + '.exe')
            result = run([compiler, '--emit-project-oop-exe', consumer, '-' + level, '-o', executable, cache], repo)
            if result.returncode:
                failures.append((level, 'consumer', result.returncode))
                continue
            if run([executable], repo).returncode != 42:
                failures.append((level, 'execution'))
            if level == 'O2':
                timed = root / 'Consumer-O2-cached.exe'
                result = run([compiler, '--emit-project-oop-exe', consumer, '-O2', '--time', cache,
                              '-o', timed], repo)
                match = re.search(r'artifact-cache-hits:\s*(\d+)', result.stdout)
                if result.returncode or match is None or int(match.group(1)) == 0:
                    failures.append((level, 'object-cache hit not observed'))
                elif run([timed], repo).returncode != 42:
                    failures.append((level, 'cached execution'))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
