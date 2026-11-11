"""Compile an aggregate-in operator in a library and exercise its consumer."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, root):
    result = subprocess.run([str(item) for item in command], cwd=root, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline([str(item) for item in command]),
          'EXIT', result.returncode, result.stdout, result.stderr, flush=True)
    return result.returncode


def main():
    compiler = Path(sys.argv[1]).resolve()
    source = Path(sys.argv[2]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-operator-in-binary-') as temporary:
        root = Path(temporary)
        (root / 'PairIn.ns').write_bytes(source.read_bytes())
        producer = root / 'Producer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>PairProducer</Name>'
                            '<OutputType>StaticLibrary</OutputType></PropertyGroup>'
                            '<ItemGroup><Compile Include="PairIn.ns"/></ItemGroup></Project>', encoding='utf-8')
        (root / 'Main.ns').write_text('''int Main() {
 PairIn a = new PairIn(1.0, 2.0); PairIn b = new PairIn(3.0, 4.0);
 PairIn sum = a + b; PairIn twice = a * 2.0; PairIn left = 2.0 * a;
 if (sum.x != 4.0 || sum.y != 6.0) return 1;
 if (twice.x != 2.0 || twice.y != 4.0 || left.x != 2.0 || left.y != 4.0) return 2;
 if (!(a == a) || !(a != b)) return 3;
 return a.x == 1.0 && b.y == 4.0 ? 42 : 4;
}''', encoding='utf-8')
        for producer_level in ('O0', 'O2'):
            library = root / ('PairProducer-' + producer_level + '.lib')
            if run([compiler, '--emit-project-oop-lib', producer, '-' + producer_level,
                    '-o', library, '--no-artifact-cache'], root):
                failures.append((producer_level, 'producer'))
                continue
            consumer = root / ('Consumer-' + producer_level + '.nsproj')
            consumer.write_text('<Project><PropertyGroup><Name>PairConsumer</Name></PropertyGroup><ItemGroup>'
                                '<Compile Include="Main.ns"/><LibraryReference Include="' + library.as_posix() +
                                '" Metadata="' + library.with_suffix('.nsmeta').as_posix() +
                                '"/></ItemGroup></Project>', encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / ('consumer-' + producer_level + '-' + level + '.exe')
                if run([compiler, '--emit-project-oop-exe', consumer, '-' + level,
                        '-o', exe, '--no-artifact-cache'], root):
                    failures.append((producer_level, level, 'compile'))
                elif run([exe], root) != 42:
                    failures.append((producer_level, level, 'run'))
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
