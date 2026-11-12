"""Check increment operators published by a separate NativeSharp library."""
from pathlib import Path
import subprocess
import sys
import tempfile


PRODUCER = '''public struct Counter {
 public int Value;
 public Counter(int value) { Value = value; }
 public static Counter operator ++(in Counter value) { return new Counter(value.Value + 1); }
 public static Counter operator --(in Counter value) { return new Counter(value.Value - 1); }
}
public struct Wide {
 public long Low;
 public long High;
 public Wide(long low, long high) { Low = low; High = high; }
 public static Wide operator ++(Wide value) { return new Wide(value.Low + 1l, value.High); }
 public static Wide operator --(Wide value) { return new Wide(value.Low - 1l, value.High); }
}
public struct RefWide {
 public long Low;
 public long High;
 public RefWide(long low, long high) { Low = low; High = high; }
 public static RefWide operator ++(ref RefWide value) {
  value.Low = value.Low + 100l;
  return new RefWide(value.Low + 1l, value.High);
 }
}
'''
CONSUMER = '''class Program {
 static int Hits;
 static int Index() { Hits = Hits + 1; return 0; }
 public static int Main() {
  Counter[] values = new Counter[1];
  values[0] = new Counter(10);
  Counter old = values[Index()]++;
  Counter next = --values[Index()];
  Wide[] wide = new Wide[1];
  wide[0] = new Wide(10l, 32l);
  Wide wideOld = wide[Index()]++;
  Wide wideNext = --wide[Index()];
  RefWide[] refs = new RefWide[1];
  refs[0] = new RefWide(10l, 32l);
  RefWide refOld = refs[Index()]++;
  return old.Value == 10 && next.Value == 10 && values[0].Value == 10 &&
   wideOld.Low == 10l && wideOld.High == 32l && wideNext.Low == 10l &&
   wideNext.High == 32l && wide[0].Low == 10l && refOld.Low == 10l &&
   refOld.High == 32l && refs[0].Low == 111l && Hits == 5 ? 42 : 1;
 }
}
'''


def run(command, root):
    result = subprocess.run([str(value) for value in command], cwd=root, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline([str(value) for value in command]),
          'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-binary-increment-') as temporary:
        root = Path(temporary)
        (root / 'Producer.ns').write_text(PRODUCER, encoding='utf-8')
        producer = root / 'Producer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>IncrementProducer</Name>'
                            '<OutputType>StaticLibrary</OutputType></PropertyGroup>'
                            '<ItemGroup><Compile Include="Producer.ns"/></ItemGroup></Project>', encoding='utf-8')
        library = root / 'IncrementProducer.lib'
        if run([compiler, '--emit-project-oop-lib', producer, '-O0', '-o', library,
                '--no-artifact-cache'], root).returncode:
            return 1
        (root / 'Main.ns').write_text(CONSUMER, encoding='utf-8')
        consumer = root / 'Consumer.nsproj'
        consumer.write_text('<Project><PropertyGroup><Name>IncrementConsumer</Name></PropertyGroup>'
                            '<ItemGroup><Compile Include="Main.ns"/><LibraryReference Include="' +
                            library.as_posix() + '" Metadata="' + library.with_suffix('.nsmeta').as_posix() +
                            '"/></ItemGroup></Project>', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / (level + '.exe')
            if run([compiler, '--emit-project-oop-exe', consumer, '-' + level, '-o', executable,
                    '--no-artifact-cache'], root).returncode:
                failures.append((level, 'compile'))
                continue
            result = run([executable], root)
            if result.returncode != 42:
                failures.append((level, 'execute', result.returncode))
    if failures:
        print('FAIL', failures, file=sys.stderr, flush=True)
        return 1
    print('operator_increment_binary: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
