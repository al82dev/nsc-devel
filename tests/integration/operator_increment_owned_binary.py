"""Check owned class increment/decrement across a NativeSharp library boundary."""
from pathlib import Path
import subprocess
import sys
import tempfile

PRODUCER = '''public class Box {
 public static int Drops;
 public int Value;
 public Box(int value) { Value = value; }
 ~Box() { Drops = Drops + 1; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
 public static owned Box operator --(Box value) { return new Box(value.Value - 1); }
 public static int Count() { return Drops; }
}
'''
CONSUMER = '''class Program {
 static int Read(borrowed Box value) { return value.Value; }
 static int Consume(owned Box value) { return value.Value; }
 static owned Box Transfer() { raii Box value = new Box(40); return value++; }
 public static int Main() {
  {
   raii Box value = new Box(10);
   {
    raii Box old = value++;
    if (old.Value != 10 || value.Value != 11 || Box.Count() != 0) return 1;
    {
     borrowed Box next = --value;
     if (next.Value != 10 || Box.Count() != 1) return 2;
    }
   }
   if (Box.Count() != 2) return 3;
  }
  if (Box.Count() != 3) return 4;
  {
   raii Box value = new Box(20);
   int previous = Read(value++);
   if (previous != 20 || value.Value != 21 || Box.Count() != 4) return 5;
   value++;
   if (value.Value != 22 || Box.Count() != 5) return 6;
  }
  if (Box.Count() != 6) return 7;
  {
   raii Box value = new Box(30);
   int previous = (value++).Value;
   if (previous != 30 || value.Value != 31 || Box.Count() != 7) return 8;
  }
  if (Box.Count() != 8) return 9;
  {
   raii Box old = Transfer();
   if (old.Value != 40 || Box.Count() != 9) return 10;
  }
  if (Box.Count() != 10) return 11;
  {
   raii Box value = new Box(50);
   int previous = Consume(value++);
   if (previous != 50 || value.Value != 51 || Box.Count() != 11) return 12;
  }
  return Box.Count() == 12 ? 42 : 13;
 }
}
'''
ACTIVE_BORROW = '''class Program {
 public static int Main() {
  raii Box value = new Box(10);
  borrowed Box view = value;
  value++;
  return view.Value;
 }
}
'''
PINNED_OWNER = '''class Program {
 public static int Main() {
  pin raii Box stable = new Box(10);
  stable++;
  return 42;
 }
}
'''
EXTRA_NEGATIVES = ()


def run(command, root):
    result = subprocess.run([str(value) for value in command], cwd=root, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline([str(value) for value in command]),
          'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-owned-binary-increment-') as temporary:
        root = Path(temporary)
        (root / 'Producer.ns').write_text(PRODUCER, encoding='utf-8')
        producer = root / 'Producer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>OwnedIncrementProducer</Name>'
                            '<OutputType>StaticLibrary</OutputType></PropertyGroup>'
                            '<ItemGroup><Compile Include="Producer.ns"/></ItemGroup></Project>', encoding='utf-8')
        consumer = root / 'Consumer.nsproj'
        for producer_level in ('O0', 'O2'):
            library = root / ('OwnedIncrementProducer-' + producer_level + '.lib')
            if run([compiler, '--emit-project-oop-lib', producer, '-' + producer_level, '-o', library,
                    '--no-artifact-cache'], root).returncode:
                failures.append((producer_level, 'producer compile'))
                continue
            consumer.write_text('<Project><PropertyGroup><Name>OwnedIncrementConsumer</Name></PropertyGroup>'
                                '<ItemGroup><Compile Include="Main.ns"/><LibraryReference Include="' +
                                library.as_posix() + '" Metadata="' + library.with_suffix('.nsmeta').as_posix() +
                                '"/></ItemGroup></Project>', encoding='utf-8')
            (root / 'Main.ns').write_text(CONSUMER, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (producer_level + '-' + level + '.exe')
                if run([compiler, '--emit-project-oop-exe', consumer, '-' + level, '-o', executable,
                        '--no-artifact-cache'], root).returncode:
                    failures.append((producer_level, level, 'compile'))
                    continue
                result = run([executable], root)
                if result.returncode != 42:
                    failures.append((producer_level, level, 'execute', result.returncode))
            for name, source, expected in (
                ('active-borrow', ACTIVE_BORROW, 'cannot overwrite owner'),
                ('pinned-owner', PINNED_OWNER, 'cannot replace pinned owner'),
            ) + EXTRA_NEGATIVES:
                (root / 'Main.ns').write_text(source, encoding='utf-8')
                result = run([compiler, '--check-project-method-bodies', consumer], root)
                if result.returncode == 0 or expected not in (result.stdout + result.stderr):
                    failures.append((producer_level, name, 'unexpected diagnostic'))
    if failures:
        print('FAIL', failures, file=sys.stderr, flush=True)
        return 1
    print('operator_increment_owned_binary: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
