"""Directed regression for trait implementations on generic value types."""
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1]).resolve()
    semantic_only = '--check' in sys.argv[2:]
    sources = {
        'struct': '''trait HasValue { int Value(); }
struct Wrapper<T> { public int n; }
impl<T> HasValue for Wrapper<T> { int Value() { return this.n; } }
class Program {
 static int Read<T>(T value) where T : HasValue { return value.Value(); }
 public static int Main() { Wrapper<int> item; item.n = 42; return Read<Wrapper<int>>(item); }
}''',
        'class': '''trait HasValue { int Value(); }
class Wrapper<T> { public int n; }
impl<T> HasValue for Wrapper<T> { int Value() { return this.n; } }
class Program {
 static int Read<T>(T value) where T : HasValue { return value.Value(); }
 public static int Main() { Wrapper<int> item = new Wrapper<int>(); item.n = 42;
  return Read<Wrapper<int>>(item); }
}''',
    }
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-generic-struct-impl-') as temporary:
        root = Path(temporary)
        for name, source in sources.items():
            case = root / name
            case.mkdir()
            (case / 'Main.ns').write_text(source, encoding='utf-8')
            project = case / 'Probe.nsproj'
            project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language>'
                               '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                               '</ItemGroup></Project>', encoding='utf-8')
            modes = ('--check-project-method-bodies',) if semantic_only else ('O0', 'O2')
            for mode in modes:
                if semantic_only:
                    command = [str(compiler), mode, str(project)]
                else:
                    executable = case / (name + '-' + mode + '.exe')
                    command = [str(compiler), '--emit-project-oop-exe', str(project),
                               '-' + mode, '-o', str(executable), '--no-artifact-cache']
                result = subprocess.run(command, cwd=case, capture_output=True, text=True,
                                        errors='replace', timeout=120)
                print(name, mode, 'compile=' + str(result.returncode), flush=True)
                if result.returncode != 0:
                    failures.append((name, mode, result.stdout + result.stderr))
                    continue
                if not semantic_only:
                    result = subprocess.run([str(executable)], cwd=case, capture_output=True,
                                            text=True, errors='replace', timeout=120)
                    print(name, mode, 'run=' + str(result.returncode), flush=True)
                    if result.returncode != 42:
                        failures.append((name, mode, result.stdout + result.stderr))
        if not semantic_only:
            negatives = {
                'orphan': ('''trait HasValue { int Value(); }
impl<T> HasValue for Missing<T> { int Value() { return 42; } }
class Program { public static int Main() { return 42; } }''',
                           'sealed trait coherence'),
                'duplicate': ('''trait HasValue { int Value(); } struct Wrapper<T> {}
impl<T> HasValue for Wrapper<T> { int Value() { return 42; } }
impl<T> HasValue for Wrapper<T> { int Value() { return 42; } }
class Program { public static int Main() { return 42; } }''', 'duplicate trait impl'),
                'condition-mismatch': ('''trait HasValue { int Value(); } trait Bound {}
struct Wrapper<T> {}
impl<T> HasValue for Wrapper<T> where T : Bound { int Value() { return 42; } }
class Program { public static int Main() { return 42; } }''',
                                       'where constraints exactly'),
            }
            for name, (source, diagnostic) in negatives.items():
                case = root / name
                case.mkdir()
                (case / 'Main.ns').write_text(source, encoding='utf-8')
                project = case / 'Probe.nsproj'
                project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language>'
                                   '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                                   '</ItemGroup></Project>', encoding='utf-8')
                result = subprocess.run([str(compiler), '--check-project-method-bodies', str(project)],
                                        cwd=case, capture_output=True, text=True,
                                        errors='replace', timeout=120)
                output = result.stdout + result.stderr
                print(name, 'rejected=' + str(result.returncode != 0), flush=True)
                if result.returncode == 0 or diagnostic not in output:
                    failures.append((name, 'semantic', output))

            producer = root / 'producer'
            consumer = root / 'consumer'
            producer.mkdir()
            consumer.mkdir()
            (producer / 'Producer.ns').write_text('''public trait HasValue { int Value(); }
public struct Wrapper<T> { public int n; }
impl<T> HasValue for Wrapper<T> { int Value() { return this.n; } }
''', encoding='utf-8')
            producer_project = producer / 'Producer.nsproj'
            producer_project.write_text('<Project><PropertyGroup><Name>TraitProducer</Name>'
                                        '<Language>3</Language><OutputType>Library</OutputType>'
                                        '</PropertyGroup><ItemGroup><Compile Include="Producer.ns" />'
                                        '</ItemGroup></Project>', encoding='utf-8')
            library = producer / 'Producer.lib'
            result = subprocess.run([str(compiler), '--emit-project-oop-lib', str(producer_project),
                                     '-O0', '-o', str(library), '--no-artifact-cache'], cwd=producer,
                                    capture_output=True, text=True, errors='replace', timeout=120)
            print('binary-producer', 'compile=' + str(result.returncode), flush=True)
            if result.returncode != 0:
                failures.append(('binary-producer', 'compile', result.stdout + result.stderr))
            else:
                (consumer / 'Main.ns').write_text('''class Program {
 static int Read<T>(T value) where T : HasValue { return value.Value(); }
 public static int Main() { Wrapper<int> item; item.n = 42; return Read<Wrapper<int>>(item); }
}''', encoding='utf-8')
                consumer_project = consumer / 'Consumer.nsproj'
                consumer_project.write_text('<Project><PropertyGroup><Name>Consumer</Name>'
                                            '<Language>3</Language></PropertyGroup><ItemGroup>'
                                            '<Compile Include="Main.ns" /><LibraryReference Include="' +
                                            library.as_posix() + '" Metadata="' +
                                            library.with_suffix('.nsmeta').as_posix() + '" />'
                                            '</ItemGroup></Project>', encoding='utf-8')
                for mode in ('O0', 'O2'):
                    executable = consumer / ('consumer-' + mode + '.exe')
                    result = subprocess.run([str(compiler), '--emit-project-oop-exe',
                                             str(consumer_project), '-' + mode, '-o', str(executable),
                                             '--no-artifact-cache'], cwd=consumer, capture_output=True,
                                            text=True, errors='replace', timeout=120)
                    print('binary-consumer', mode, 'compile=' + str(result.returncode), flush=True)
                    if result.returncode != 0:
                        failures.append(('binary-consumer', mode, result.stdout + result.stderr))
                        continue
                    result = subprocess.run([str(executable)], cwd=consumer, capture_output=True,
                                            text=True, errors='replace', timeout=120)
                    print('binary-consumer', mode, 'run=' + str(result.returncode), flush=True)
                    if result.returncode != 42:
                        failures.append(('binary-consumer', mode, result.stdout + result.stderr))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
