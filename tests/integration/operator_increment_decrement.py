"""Exercise user-defined increment/decrement without reevaluating an indexed target."""
from pathlib import Path
import subprocess
import sys
import tempfile


SOURCE = '''struct Counter {
 public int Value;
 public Counter(int value) { Value = value; }
 public static Counter operator ++(Counter value) { return new Counter(value.Value + 1); }
 public static Counter operator --(Counter value) { return new Counter(value.Value - 1); }
}
class Program {
 static int Hits;
 static int Index() { Hits = Hits + 1; return 0; }
 public static int Main() {
  Counter[] values = new Counter[1];
  values[0] = new Counter(10);
  Counter a = values[Index()]++;
  Counter b = ++values[Index()];
  Counter c = values[Index()]--;
  Counter d = --values[Index()];
  if (a.Value != 10 || b.Value != 12 || c.Value != 12 || d.Value != 10) return 1;
  if (values[0].Value != 10 || Hits != 4) return 2;
  return 42;
 }
}
'''

WIDE = '''struct Wide {
 public long Low;
 public long High;
 public Wide(long low, long high) { Low = low; High = high; }
 public static Wide operator ++(Wide value) { return new Wide(value.Low + 1l, value.High); }
 public static Wide operator --(Wide value) { return new Wide(value.Low - 1l, value.High); }
}
class Program {
 static int Hits;
 static int Index() { Hits = Hits + 1; return 0; }
 public static int Main() {
  Wide[] values = new Wide[1];
  values[0] = new Wide(10l, 32l);
  Wide a = values[Index()]++;
  Wide b = --values[Index()];
  return a.Low == 10l && a.High == 32l && b.Low == 10l && b.High == 32l &&
   values[0].Low == 10l && Hits == 2 ? 42 : 1;
 }
}
'''

REF_MUTATION = '''struct Counter {
 public int Value;
 public Counter(int value) { Value = value; }
 public static Counter operator ++(ref Counter value) {
  value.Value = value.Value + 100;
  return new Counter(value.Value + 1);
 }
}
class Program {
 static int Hits;
 static int Index() { Hits = Hits + 1; return 0; }
 public static int Main() {
  Counter[] values = new Counter[1];
  values[0] = new Counter(10);
  Counter old = values[Index()]++;
  return old.Value == 10 && values[0].Value == 111 && Hits == 1 ? 42 : 1;
 }
}
'''


INVALID = '''struct Counter {
 public int Value;
 public Counter(int value) { Value = value; }
 public static Counter operator ++(Counter value) { return value; }
 public static int operator --(Counter value) { return 0; }
}
class Program {
 static int Readonly(in Counter value) { value++; return 0; }
 static int WrongReturn() { Counter value = new Counter(1); value--; return 0; }
 public static int Main() { return 42; }
}
'''


def main():
    compiler = Path(sys.argv[1]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-user-increment-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>UserIncrement</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
        variants = (
            ('small', SOURCE),
            ('wide', WIDE),
            ('small-in', SOURCE.replace('operator ++(Counter value)', 'operator ++(in Counter value)')
                         .replace('operator --(Counter value)', 'operator --(in Counter value)')),
            ('wide-in', WIDE.replace('operator ++(Wide value)', 'operator ++(in Wide value)')
                        .replace('operator --(Wide value)', 'operator --(in Wide value)')),
            ('small-ref', SOURCE.replace('operator ++(Counter value)', 'operator ++(ref Counter value)')
                          .replace('operator --(Counter value)', 'operator --(ref Counter value)')),
            ('wide-ref', WIDE.replace('operator ++(Wide value)', 'operator ++(ref Wide value)')
                         .replace('operator --(Wide value)', 'operator --(ref Wide value)')),
            ('ref-mutation', REF_MUTATION),
        )
        for name, source in variants:
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                build = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                       errors='replace', timeout=120)
                print('COMMAND', subprocess.list2cmdline(command), 'EXIT', build.returncode,
                      build.stdout + build.stderr, flush=True)
                if build.returncode:
                    failures.append((name, level, 'compile'))
                    continue
                result = subprocess.run([str(executable)], cwd=root, capture_output=True, text=True,
                                        errors='replace', timeout=120)
                print('EXECUTE', name, level, 'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
                if result.returncode != 42:
                    failures.append((name, level, 'execute', result.returncode))
        (root / 'Main.ns').write_text(INVALID, encoding='utf-8')
        command = [str(compiler), '--check-project-method-bodies', str(project)]
        result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                errors='replace', timeout=120)
        output = result.stdout + result.stderr
        print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode, output, flush=True)
        if (result.returncode == 0 or 'requires a modifiable operand' not in output or
                'result must be assignable to its operand' not in output):
            failures.append(('invalid', 'unexpected diagnostic'))
    if failures:
        print('FAIL', failures, file=sys.stderr, flush=True)
        return 1
    print('operator_increment_decrement: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
