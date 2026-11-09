import sys
import tempfile
from pathlib import Path
import subprocess


def run(command, cwd):
    return subprocess.run([str(part) for part in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler = Path(sys.argv[1]).resolve()
    library = Path(sys.argv[2]).resolve()
    metadata = library.with_suffix('.nsmeta')
    assert library.is_file() and metadata.is_file(), ('missing binary SDK', library, metadata)
    with tempfile.TemporaryDirectory(prefix='nsc-shared-binary-') as temporary:
        root = Path(temporary)
        project = root / 'Consumer.nsproj'
        project.write_text('<Project><PropertyGroup><Name>BinarySharedConsumer</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" />'
                           '<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                           metadata.as_posix() + '" /></ItemGroup></Project>', encoding='utf-8')
        source = root / 'Program.ns'
        source.write_text('''class Payload { public long Data; }
class Program { public static int Main() { Payload value = new Payload(); value.Data = 9l; return 0; } }
''', encoding='utf-8')
        result = run([compiler, '--check-project-method-bodies', project], root)
        assert result.returncode == 0, ('mutable control rejected', result.stdout + result.stderr)
        print('ordinary mutable class accepted', flush=True)
        source.write_text('''using NativeSharp.Core;
class Payload { public static int Drops; public long Data; public Payload() { Data = 42l; }
 ~Payload() { Drops = Drops + 1; } }
struct Value { public static int Drops; public long Data; public Value() { Data = 42l; }
 ~Value() { Drops = Drops + 1; } }
class Program { public static int Main() {
 WeakRc<Payload> localWeak;
 { Rc<Payload> owner = SharedPointers.MakeRc<Payload>(); localWeak = owner.Downgrade();
   Rc<Payload> clone = localWeak.Upgrade();
   if (owner.IsEmpty() || clone.IsEmpty() || owner.StrongCount() != 2l ||
       owner.WeakCount() != 1l || clone.Get().Data != 42l) return 1; }
 if (!localWeak.Expired() || Payload.Drops != 1 || !localWeak.Upgrade().IsEmpty()) return 2;
 WeakArc<Payload> atomicWeak;
 { Arc<Payload> owner = SharedPointers.MakeArc<Payload>(); atomicWeak = owner.Downgrade();
   Arc<Payload> clone = atomicWeak.Upgrade();
   if (owner.IsEmpty() || clone.IsEmpty() || owner.StrongCount() != 2l ||
       owner.WeakCount() != 1l || clone.Get().Data != 42l) return 3; }
 if (!atomicWeak.Expired() || Payload.Drops != 2 || !atomicWeak.Upgrade().IsEmpty()) return 4;
 WeakRc<Value> localValueWeak;
 { Rc<Value> owner = SharedPointers.MakeRc<Value>(); localValueWeak = owner.Downgrade();
   Rc<Value> clone = localValueWeak.Upgrade(); if (clone.Get().Data != 42l) return 5; }
 if (!localValueWeak.Expired() || Value.Drops != 1) return 6;
 WeakArc<Value> atomicValueWeak;
 { Arc<Value> owner = SharedPointers.MakeArc<Value>(); atomicValueWeak = owner.Downgrade();
   Arc<Value> clone = atomicValueWeak.Upgrade(); if (clone.Get().Data != 42l) return 7; }
 if (!atomicValueWeak.Expired() || Value.Drops != 2) return 8;
 return 42;
} }
''', encoding='utf-8')
        for level in ('O0', 'O2'):
            exe = root / ('consumer-' + level + '.exe')
            result = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', exe], root)
            assert result.returncode == 0, ('compile', level, result.stdout + result.stderr)
            result = run([exe], root)
            assert result.returncode == 42, ('run', level, result.returncode, result.stdout, result.stderr)
            print('binary shared owners', level, 'exit=42', flush=True)
        for family, weak, make in (('Rc', 'WeakRc', 'MakeRc'), ('Arc', 'WeakArc', 'MakeArc')):
            source.write_text(('''using NativeSharp.Core;
class Payload { public Payload() { } }
class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
class Holder { public WEAK<Payload> value; public int number;
 public void Initialize(WEAK<Payload> weak) { value = move(weak); }
 public Holder(WEAK<Payload> weak) { Initialize(move(weak)); number = Gate.Read(); } }
class Program { public static int Main() { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 int caught = 0; try { Holder holder = new Holder(owner.Downgrade()); } catch (Exception error) { caught = 1; }
 return caught == 1 && owner.WeakCount() == 0l ? 42 : (int)owner.WeakCount() + 10 * caught; } }
''').replace('WEAK', weak).replace('STRONG', family).replace('MAKE', make), encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / ('binary-constructor-' + family + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', exe], root)
                assert result.returncode == 0, ('constructor compile', family, level, result.stdout + result.stderr)
                result = run([exe], root)
                print('binary constructor cleanup', family, level, 'exit=' + str(result.returncode), flush=True)
                assert result.returncode == 42, ('constructor cleanup', family, level, result.returncode)
        source.write_text('''using NativeSharp.Core;
class Payload { public long Data; }
class Program { public static int Main() {
 Rc<Payload> owner = SharedPointers.MakeRc<Payload>(); borrowed Payload value = owner.Get();
 value.Data = 9l; return 0;
} }
''', encoding='utf-8')
        result = run([compiler, '--check-project-method-bodies', project], root)
        output = result.stdout + result.stderr
        assert result.returncode != 0 and 'error:' in output and 'value.Data' in output and (
            'readonly' in output or 'not assignable' in output), (
            'readonly mutation accepted or failed for another reason', output)
        print('binary readonly mutation rejected', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
