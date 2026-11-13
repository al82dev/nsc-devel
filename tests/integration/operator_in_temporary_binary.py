"""Exercise `in` operator temporaries against a separately compiled library."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, cwd):
    command = [str(part) for part in command]
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                            errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          '\n', result.stdout, result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    original = Path(sys.argv[2]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-in-operator-binary-') as temporary:
        root = Path(temporary)
        (root / 'Rational.ns').write_bytes((original / 'Rational.ns').read_bytes())
        (root / 'Tracked.ns').write_text('''namespace NativeSharp.Core { public class Exception { } }
public class State { public static int Log; }
public struct Tracked {
 public int Id;
 public Tracked(int id) { Id = id; State.Log = State.Log * 10 + id; }
 ~Tracked() { State.Log = State.Log * 10 + Id + 3; }
 public static bool operator ==(in Tracked a, in Tracked b) {
  State.Log = State.Log * 10 + 3; return a.Id == 1 && b.Id == 2;
 }
 public static bool operator !=(in Tracked a, in Tracked b) { return !(a == b); }
}
class Part {
 public int Id;
 public Part(int id) { Id = id; State.Log = State.Log * 10 + id; }
 ~Part() { State.Log = State.Log * 10 + Id + 3; }
}
public struct FieldOwner {
 private owned Part item;
 public FieldOwner(int id) { item = new Part(id); }
 public static bool operator ==(in FieldOwner a, in FieldOwner b) {
  State.Log = State.Log * 10 + 3; return a.item.Id == 1 && b.item.Id == 2;
 }
 public static bool operator !=(in FieldOwner a, in FieldOwner b) { return !(a == b); }
}
public struct FaultOwner {
 private owned Part item;
 public FaultOwner(int id) throws NativeSharp.Core.Exception {
  item = new Part(id);
  if (id == 2) throw new NativeSharp.Core.Exception();
 }
 public static bool operator ==(in FaultOwner a, in FaultOwner b) { return a.item.Id == b.item.Id; }
 public static bool operator !=(in FaultOwner a, in FaultOwner b) { return !(a == b); }
}
''', encoding='utf-8')
        producer = root / 'Producer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>RationalProducer</Name>'
                           '<OutputType>StaticLibrary</OutputType></PropertyGroup>'
                           '<ItemGroup><Compile Include="Rational.ns"/><Compile Include="Tracked.ns"/>'
                           '</ItemGroup></Project>', encoding='utf-8')
        library = root / 'RationalProducer.lib'
        result = run([compiler, '--emit-project-oop-lib', producer, '-O0', '-o', library,
                      '--no-artifact-cache'], root)
        if result.returncode:
            raise AssertionError('producer failed')
        metadata = library.with_suffix('.nsmeta')
        if not library.is_file() or not metadata.is_file():
            raise AssertionError('producer artifacts missing')
        consumer = root / 'Consumer.nsproj'
        consumer.write_text('<Project><PropertyGroup><Name>RationalConsumer</Name></PropertyGroup><ItemGroup>'
                            '<Compile Include="Main.ns"/><LibraryReference Include="' +
                            library.as_posix() + '" Metadata="' + metadata.as_posix() +
                            '"/></ItemGroup></Project>', encoding='utf-8')
        cases = {
            'locals': 'Rational a = new Rational(1,1); Rational b = new Rational(2,2); return a == b ? 42 : 1;',
            'one-temporary': 'Rational a = new Rational(1,1); return a == new Rational(2,2) ? 42 : 2;',
            'two-temporaries': 'return new Rational(1,1) == new Rational(2,2) ? 42 : 3;',
            'destructor-temporaries': 'bool ok = (new Tracked(1) == new Tracked(2)) && State.Log == 123; '
                                      'if (!ok) return 10; return State.Log == 12354 ? 42 : 11;',
            'destructor-locals': '{ Tracked a = new Tracked(1); Tracked b = new Tracked(2); '
                                 'bool ok = a == b; if (!ok || State.Log != 123) return 12; } '
                                 'return State.Log == 12354 ? 42 : 13;',
            'private-owned-temporaries': 'bool ok = (new FieldOwner(1) == new FieldOwner(2)) && State.Log == 123; '
                                         'if (!ok) return 14; return State.Log == 12354 ? 42 : 15;',
            'private-owned-locals': '{ FieldOwner a = new FieldOwner(1); FieldOwner b = new FieldOwner(2); '
                                    'bool ok = a == b; if (!ok || State.Log != 123) return 16; } '
                                    'return State.Log == 12354 ? 42 : 17;',
            'constructor-throw-locals': 'int caught = 0; try { FaultOwner a = new FaultOwner(1); '
                                        'FaultOwner b = new FaultOwner(2); } '
                                        'catch (NativeSharp.Core.Exception e) { caught = 1; } '
                                        'return caught == 1 && State.Log == 1254 ? 42 : 18;',
            'constructor-throw-temporaries': 'int caught = 0; try { bool value = '
                                             'new FaultOwner(1) == new FaultOwner(2); } '
                                             'catch (NativeSharp.Core.Exception e) { caught = 1; } '
                                             'return caught == 1 && State.Log == 1254 ? 42 : 19;',
        }
        for name, body in cases.items():
            (root / 'Main.ns').write_text('int Main() { ' + body + ' }', encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', consumer, '-' + level,
                              '-o', executable, '--no-artifact-cache'], root)
                if result.returncode:
                    failures.append((name, level, 'consumer compile'))
                    continue
                result = run([executable], root)
                if result.returncode != 42:
                    failures.append((name, level, 'execute', result.returncode))
        copies = {
            'copy-plain': ('Rational a = new Rational(1,1); Rational b = a; return 42;', True),
            'copy-destructor': ('Tracked a = new Tracked(1); Tracked b = a; return 42;', False),
            'copy-private-owner': ('FieldOwner a = new FieldOwner(1); FieldOwner b = a; return 42;', False),
        }
        for name, (body, valid) in copies.items():
            (root / 'Main.ns').write_text('int Main() { ' + body + ' }', encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', consumer, '--no-banner'], root)
            if (result.returncode == 0) != valid or (
                    not valid and 'move-only value' not in result.stdout + result.stderr):
                failures.append((name, 'unexpected copyability', result.returncode))
        # Inequality calls the independently checked equality implementation.
        (root / 'Main.ns').write_text(
            'int Main() { bool value = new FieldOwner(1) != new FieldOwner(2); '
            'return !value && State.Log == 12354 ? 42 : 1; }',
            encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / ('calling-operator-' + level + '.exe')
            result = run([compiler, '--emit-project-oop-exe', consumer, '-' + level,
                          '-o', executable, '--no-artifact-cache'], root)
            if result.returncode or run([executable], root).returncode != 42:
                failures.append(('certified-calling-operator', level, 'consumer'))
        (root / 'Main.ns').write_text('''int Main() {
 bool ok = (new Tracked(1) == new Tracked(2)) && State.Log == 123;
 if (!ok) return 1;
 return State.Log == 12354 ? 54 : State.Log == 12365 ? 65 : 2;
}''', encoding='utf-8')
        cache_option = '--artifact-cache=' + (root / 'consumer-cache').as_posix()
        cached_executable = root / 'cached-consumer.exe'
        for phase, expected in (('initial', 54), ('repeated', 54), ('producer-changed', 65)):
            if phase == 'producer-changed':
                tracked = root / 'Tracked.ns'
                text = tracked.read_text(encoding='utf-8')
                if 'Id + 3;' not in text:
                    raise AssertionError('tracked destructor marker missing')
                tracked.write_text(text.replace('Id + 3;', 'Id + 4;', 1), encoding='utf-8')
                result = run([compiler, '--emit-project-oop-lib', producer, '-O0', '-o', library,
                              '--no-artifact-cache'], root)
                if result.returncode:
                    failures.append((phase, 'producer compile'))
                    continue
            result = run([compiler, '--emit-project-oop-exe', consumer, '-O0', '-o', cached_executable,
                          cache_option, '--verbose'], root)
            if result.returncode:
                failures.append((phase, 'cached consumer compile'))
                continue
            output = result.stdout + result.stderr
            if phase == 'repeated' and 'compilation-bundle-cache: hit' not in output:
                failures.append((phase, 'bundle cache hit not observed'))
            if phase == 'producer-changed' and 'cache hit LLVM NativeSharp.OOP' not in output:
                failures.append((phase, 'LLVM cache hit not observed'))
            result = run([cached_executable], root)
            print('CACHE-PHASE', phase, 'EXIT', result.returncode, flush=True)
            if result.returncode != expected:
                failures.append((phase, 'cached consumer execute', result.returncode))
        (root / 'Main.ns').write_text('''int Main() {
 bool value = new FieldOwner(1) == new FieldOwner(2); return value ? 42 : 1;
}''', encoding='utf-8')
        certified_exe = root / 'certified-cache.exe'
        for phase in ('certified', 'uncertified'):
            if phase == 'uncertified':
                tracked = root / 'Tracked.ns'
                source = tracked.read_text(encoding='utf-8')
                before = 'State.Log = State.Log * 10 + 3; return a.item.Id == 1 && b.item.Id == 2;'
                after = 'int marker = 3; State.Log = State.Log * 10 + marker; return a.item.Id == 1 && b.item.Id == 2;'
                if source.count(before) != 1:
                    raise AssertionError('operator proof fixture missing')
                tracked.write_text(source.replace(before, after), encoding='utf-8')
                if run([compiler, '--emit-project-oop-lib', producer, '-O0', '-o', library,
                        '--no-artifact-cache'], root).returncode:
                    failures.append((phase, 'producer compile'))
                    continue
            result = run([compiler, '--emit-project-oop-exe', consumer, '-O0', '-o', certified_exe,
                          cache_option, '--verbose'], root)
            if phase == 'certified':
                if result.returncode or run([certified_exe], root).returncode != 42:
                    failures.append((phase, 'consumer'))
            elif result.returncode == 0 or 'no viable' not in result.stdout + result.stderr:
                failures.append((phase, 'expected rejection after producer change', result.returncode))
        (root / 'Main.ns').write_text(
            'int Main() { return new FieldOwner(1) != new FieldOwner(2) ? 1 : 42; }', encoding='utf-8')
        result = run([compiler, '--check-project-method-bodies', consumer, '--no-banner'], root)
        if result.returncode == 0 or 'no viable' not in result.stdout + result.stderr:
            failures.append(('uncertified-called-operator', 'expected rejection', result.returncode))
    if failures:
        print('FAIL', failures, file=sys.stderr, flush=True)
        return 1
    print('operator_in_temporary_binary: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
