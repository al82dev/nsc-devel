import os
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
            if os.environ.get('NSC_SHARED_BINARY_FAMILY') not in (None, family):
                continue
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
            throwing_source = '''using NativeSharp.Core;
class Resource { public static int Drops; public Resource() { }
 ~Resource() { Drops = Drops + 1; } }
class Bad { public owned Resource item;
 public Bad() throws Exception { item = new Resource(); throw new Exception(); } }
struct BadValue { public owned Resource item;
 public BadValue() throws Exception { item = new Resource(); throw new Exception(); } }
class Program { public static int Main() { int caught = 0;
 CLASS_CASE
 try { STRONG<BadValue> failed = SharedPointers.MAKE<BadValue>(); } catch (Exception error) { caught = caught + 1; }
 return caught == EXPECTED && Resource.Drops == EXPECTED ? 42 : 1; } }
'''
            class_case = os.environ.get('NSC_SHARED_BINARY_THROWING_CLASS', '1') != '0'
            throwing_source = throwing_source.replace('CLASS_CASE',
                'try { STRONG<Bad> failed = SharedPointers.MAKE<Bad>(); } '
                'catch (Exception error) { caught = caught + 1; }' if class_case else '')
            source.write_text(throwing_source.replace('EXPECTED', '2' if class_case else '1')
                              .replace('STRONG', family).replace('MAKE', make + 'Throwing'), encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / ('binary-throwing-' + family + '-' + level + '.exe')
                if os.environ.get('NSC_SHARED_BINARY_LEVEL') not in (None, level):
                    continue
                result = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', exe], root)
                assert result.returncode == 0, ('throwing compile', family, level, result.stdout + result.stderr)
                result = run([exe], root)
                print('binary throwing factories', family, level, 'exit=' + str(result.returncode), flush=True)
                assert result.returncode == 42, ('throwing factory', family, level, result.returncode)
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
        for family, make in (('Rc', 'MakeRc'), ('Arc', 'MakeArc')):
            for body in ('int result = owner.Get();', 'int result = 0; result = owner.Get();'):
                source.write_text(('''using NativeSharp.Core;
class CastPayload { public int Data; public CastPayload() { }
 public static implicit operator int(CastPayload value) { value.Data = 9; return 0; } }
class Program { public static int Main() { FAMILY<CastPayload> owner = SharedPointers.MAKE<CastPayload>();
 BODY return result; } }
''').replace('FAMILY', family).replace('MAKE', make).replace('BODY', body), encoding='utf-8')
                result = run([compiler, '--check-project-method-bodies', project], root)
                output = result.stdout + result.stderr
                assert result.returncode != 0 and 'readonly borrowed object cannot invoke' in output, (
                    'binary implicit readonly conversion accepted or failed for another reason', family, body, output)
                print('binary implicit readonly conversion rejected', family, body, flush=True)
            for operator in ('??=', '??'):
                source.write_text(('''using NativeSharp.Core;
class CastPayload { public int Data; public CastPayload() { }
 public static implicit operator string(CastPayload value) { value.Data = 9; return "ok"; } }
class Program { public static int Main() { FAMILY<CastPayload> owner = SharedPointers.MAKE<CastPayload>();
 string value = null; value OPERATOR owner.Get(); return 0; } }
''').replace('FAMILY', family).replace('MAKE', make).replace('OPERATOR', operator), encoding='utf-8')
                result = run([compiler, '--check-project-method-bodies', project], root)
                output = result.stdout + result.stderr
                assert result.returncode != 0 and 'readonly borrowed object cannot invoke' in output, (
                    'binary coalescing readonly conversion accepted or failed for another reason', family, operator, output)
                print('binary coalescing readonly conversion rejected', family, operator, flush=True)
            for body in ('Payload[] values = [owner.Get()];',
                         'Payload[] existing = new Payload[0]; Payload[] values = [..existing, owner.Get()];'):
                source.write_text(('''using NativeSharp.Core;
class Payload { public int Data; public Payload() { } }
class Program { public static int Main() { FAMILY<Payload> owner = SharedPointers.MAKE<Payload>();
 BODY values[0].Data = 9; return 0; } }
''').replace('FAMILY', family).replace('MAKE', make).replace('BODY', body), encoding='utf-8')
                result = run([compiler, '--check-project-method-bodies', project], root)
                output = result.stdout + result.stderr
                assert result.returncode != 0 and 'readonly borrowed' in output, (
                    'binary collection accepted readonly payload', family, body, output)
                print('binary collection readonly element rejected', family, body, flush=True)
            source.write_text(('''using NativeSharp.Core;
class Child { public int Value; public Child() { } }
class Payload { public Child[] Data; public Payload() { Data = new Child[1]; Data[0] = new Child(); } }
class Program { public static int Main() { FAMILY<Payload> owner = SharedPointers.MAKE<Payload>();
 Child[] values = [..owner.Get().Data]; values[0].Value = 9; return 0; } }
''').replace('FAMILY', family).replace('MAKE', make), encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', project], root)
            output = result.stdout + result.stderr
            assert result.returncode != 0 and 'readonly borrowed' in output, (
                'binary readonly object spread accepted', family, output)
            print('binary readonly object spread rejected', family, flush=True)
            for lifecycle, body in (
                ('constructor', 'public EscapePayload() { Alias = this; }'),
                ('destructor', 'public EscapePayload() { } ~EscapePayload() { Alias = this; }'),
                ('constructor-local', 'public EscapePayload() { EscapePayload local = this; Alias = local; }'),
                ('destructor-local', 'public EscapePayload() { } ~EscapePayload() { EscapePayload local = this; Alias = local; }'),
                ('constructor-helper', 'public EscapePayload() { Publish(); } public void Publish() { Alias = this; }'),
                ('destructor-helper', 'public EscapePayload() { } ~EscapePayload() { Publish(); } public void Publish() { Alias = this; }'),
                ('constructor-virtual', 'public EscapePayload() { Publish(); } public virtual void Publish() { Alias = this; }'),
                ('destructor-virtual', 'public EscapePayload() { } ~EscapePayload() { Publish(); } public virtual void Publish() { Alias = this; }'),
                ('constructor-static', 'public EscapePayload() { Publish(this); } public static void Publish(EscapePayload value) { Alias = value; }'),
                ('constructor-global-field', 'public EscapePayload() { Box.Value = this; }'),
                ('destructor-global-field', 'public EscapePayload() { } ~EscapePayload() { Box.Value = this; }'),
                ('constructor-global-field-local', 'public EscapePayload() { Holder local = Box; local.Value = this; }'),
                ('destructor-global-field-local', 'public EscapePayload() { } ~EscapePayload() { Holder first = Box; Holder second = first; second.Value = this; }'),
                ('constructor-global-field-nested', 'public EscapePayload() { Box.Child.Value = this; }'),
                ('destructor-global-field-nested', 'public EscapePayload() { } ~EscapePayload() { Holder local = Box; Inner child = local.Child; child.Value = this; }'),
                ('constructor-global-array', 'public EscapePayload() { ArrayBox[0] = this; }'),
                ('destructor-global-array-local', 'public EscapePayload() { } ~EscapePayload() { EscapePayload[] local = ArrayBox; local[0] = this; }'),
                ('constructor-called-holder', 'public EscapePayload() { GetBox().Value = this; }'),
                ('destructor-called-holder-local', 'public EscapePayload() { } ~EscapePayload() { Holder local = GetBox(); local.Value = this; }'),
                ('constructor-identity-global', 'public EscapePayload() { Holder local = Identity(Box); local.Value = this; }'),
                ('constructor-identity-published', 'public EscapePayload() { Holder fresh = new Holder(); Holder alias = Identity(fresh); Box = Identity(alias); Holder local = Identity(fresh); local.Value = this; }'),
                ('constructor-identity-impure', 'public EscapePayload() { Holder fresh = new Holder(); Holder local = PublishHolder(fresh); local.Value = this; }'),
                ('constructor-identity-prior-call', 'public EscapePayload() { Holder fresh = new Holder(); PublishOnly(fresh); Holder local = Identity(fresh); local.Value = this; }'),
                ('constructor-named-static', 'public EscapePayload() { Publish(value: this, ignored: 1); } public static void Publish(int ignored, EscapePayload value) { Alias = value; }'),
                ('destructor-static', 'public EscapePayload() { } ~EscapePayload() { Publish(this); } public static void Publish(EscapePayload value) { Alias = value; }'),
                ('constructor-instance-arg', 'public EscapePayload() { Helper helper = new Helper(); helper.Publish(this); }'),
                ('destructor-instance-arg', 'public Helper helper; public EscapePayload() { helper = new Helper(); } ~EscapePayload() { helper.Publish(this); }'),
                ('constructor-virtual-arg', 'public EscapePayload() { VirtualHelper helper = new VirtualHelper(); helper.Publish(this); }'),
                ('destructor-virtual-arg', 'public VirtualHelper helper; public EscapePayload() { helper = new VirtualHelper(); } ~EscapePayload() { helper.Publish(this); }'),
                ('constructor-interface-arg', 'public EscapePayload() { InterfaceHelper helper = new InterfaceHelper(); borrowed IPublisher view = helper; view.Publish(this); }'),
                ('constructor-interface-receiver', 'public EscapePayload() { Forward(this); } public static void Forward(IReceiver view) { view.Publish(); } public void Publish() { Alias = this; }'),
                ('constructor-interface-return', 'public EscapePayload() { Alias = Forward(this); } public static EscapePayload Forward(IReturn value) { return value.Self(); } public EscapePayload Self() { return this; }'),
                ('constructor-return', 'public EscapePayload() { Alias = Self(); } public EscapePayload Self() { return this; }'),
                ('constructor-property-return', 'public EscapePayload() { Alias = Self; } public EscapePayload Self { get { return this; } }'),
                ('constructor-virtual-return', 'public EscapePayload() { Alias = Self(); } public virtual EscapePayload Self() { return this; }'),
                ('destructor-virtual-return', 'public EscapePayload() { } ~EscapePayload() { Alias = Self(); } public virtual EscapePayload Self() { return this; }'),
                ('constructor-static-return', 'public EscapePayload() { Alias = Identity(this); } public static EscapePayload Identity(EscapePayload value) { return value; }'),
                ('destructor-static-return', 'public EscapePayload() { } ~EscapePayload() { Alias = Identity(this); } public static EscapePayload Identity(EscapePayload value) { return value; }'),
                ('destructor-return', 'public EscapePayload() { } ~EscapePayload() { Alias = Self(); } public EscapePayload Self() { return this; }'),
            ):
                source.write_text(('''using NativeSharp.Core;
interface IReceiver { void Publish(); }
interface IReturn { EscapePayload Self(); }
interface IPublisher { void Publish(EscapePayload value); }
class InterfaceHelper : IPublisher { public void Publish(EscapePayload value) { EscapePayload.Alias = value; } }
class Helper { public void Publish(EscapePayload value) { EscapePayload.Alias = value; } }
class VirtualHelper { public virtual void Publish(EscapePayload value) { EscapePayload.Alias = value; } }
class Inner { public EscapePayload Value; }
class Holder { public EscapePayload Value; public Inner Child; }
class EscapePayload IMPLEMENTS { public static EscapePayload Alias; public static Holder Box;
 public static EscapePayload[] ArrayBox; public static Holder GetBox() { return Box; }
 public static Holder Identity(Holder input) { return input; }
 public static Holder PublishHolder(Holder input) { Box = input; return input; }
 public static void PublishOnly(Holder input) { Box = input; } BODY }
class Program { public static int Main() { FAMILY<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }
''').replace('FAMILY', family).replace('MAKE', make).replace('BODY', body)
                    .replace('IMPLEMENTS', ': IReceiver' if lifecycle == 'constructor-interface-receiver'
                             else ': IReturn' if lifecycle == 'constructor-interface-return' else ''),
                    encoding='utf-8')
                for level in ('O0', 'O2'):
                    exe = root / ('binary-published-this-' + family + '-' + lifecycle + '-' + level + '.exe')
                    result = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', exe], root)
                    output = result.stdout + result.stderr
                    assert result.returncode != 0 and 'shared payload publishes its receiver' in output, (
                        'binary shared receiver publication accepted or failed for another reason',
                        family, lifecycle, level, output)
                    print('binary shared receiver publication rejected', family, lifecycle, level, flush=True)
        for family, make in (('Rc', 'MakeRc'), ('Arc', 'MakeArc')):
            source.write_text(('''using NativeSharp.Core;
class Holder { public SafePayload Value; }
class SafePayload { public int Data;
 public static Holder Identity(Holder input) { return input; }
 public SafePayload() { Holder fresh = new Holder(); Holder local = Identity(fresh);
  local.Value = this; Data = 42; } }
class Program { public static int Main() { FAMILY<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.Get().Data; } }
''').replace('FAMILY', family).replace('MAKE', make), encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / ('binary-local-identity-' + family + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', exe], root)
                assert result.returncode == 0, ('binary local identity compile', family, level, result.stdout + result.stderr)
                result = run([exe], root)
                assert result.returncode == 42, ('binary local identity run', family, level, result.returncode)
                print('binary local identity', family, level, 'exit=42', flush=True)
        source.write_text('''using NativeSharp.Core;
class Input { public Input() { } public static implicit operator int(Input value) { return 42; } }
class Bucket { public static int Sum; public static owned Bucket CreateBuilder() { return new Bucket(); }
 public void Add(int value) { Sum = Sum + value; } public Bucket ToCollection() { return this; } }
class Program { public static int Main() { Input item = new Input(); borrowed Input alias = item;
 Bucket result = [alias]; return Bucket.Sum; } }
''', encoding='utf-8')
        for level in ('O0', 'O2'):
            exe = root / ('binary-builder-' + level + '.exe')
            result = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', exe], root)
            assert result.returncode == 0, ('binary collection builder compile', level, result.stdout + result.stderr)
            result = run([exe], root)
            assert result.returncode == 42, ('binary collection builder run', level, result.returncode)
            print('binary collection builder conversion', level, 'exit=42', flush=True)
        producer = root / 'PayloadProducer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>PayloadProducer</Name><Language>3</Language>'
                            '<OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup>'
                            '<Compile Include="PayloadProducer.ns" /><LibraryReference Include="' +
                            library.as_posix() + '" Metadata="' + metadata.as_posix() +
                            '" /></ItemGroup></Project>', encoding='utf-8')
        (root / 'PayloadProducer.ns').write_text('''public class EscapingPayload {
 public static EscapingPayload Alias; public EscapingPayload() { Alias = this; } }
public class SafePayload { public int Data; public SafePayload() { Data = 42; } }
public struct BinaryValue { public int Data; public BinaryValue() { Data = 42; } }
''', encoding='utf-8')
        payload_library = root / 'PayloadProducer.lib'
        result = run([compiler, '--emit-project-oop-lib', producer, '-O0', '-o', payload_library], root)
        assert result.returncode == 0, ('binary payload producer', result.stdout + result.stderr)
        project.write_text('<Project><PropertyGroup><Name>BinarySharedConsumer</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" />'
                           '<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                           metadata.as_posix() + '" /><LibraryReference Include="' +
                           payload_library.as_posix() + '" Metadata="' +
                           payload_library.with_suffix('.nsmeta').as_posix() +
                           '" /></ItemGroup></Project>', encoding='utf-8')
        for family, make in (('Rc', 'MakeRc'), ('Arc', 'MakeArc')):
            for payload in ('EscapingPayload', 'SafePayload', 'BinaryValue'):
                source.write_text(('''using NativeSharp.Core;
class Program { public static int Main() { FAMILY<PAYLOAD> owner = SharedPointers.MAKE<PAYLOAD>();
BODY } }
''').replace('FAMILY', family).replace('MAKE', make).replace('PAYLOAD', payload).replace('BODY',
                    'return owner.Get().Data == 42 ? 42 : 1;' if payload != 'EscapingPayload' else 'return 42;'),
                    encoding='utf-8')
                for level in ('O0', 'O2'):
                    exe = root / ('binary-payload-' + family + '-' + payload + '-' + level + '.exe')
                    result = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', exe], root)
                    output = result.stdout + result.stderr
                    if payload != 'EscapingPayload':
                        assert result.returncode == 0, ('binary safe payload compile', family, level, output)
                        result = run([exe], root)
                        assert result.returncode == 42, ('binary safe payload run', family, level, result.returncode)
                    else:
                        assert result.returncode != 0 and 'shared payload class has no verifiable precompiled lifecycle body' in output, (
                            'binary class payload accepted or failed for another reason', family, payload, level, output)
                    print('binary precompiled payload', family, payload, level, 'verified', flush=True)
            source.write_text(('''using NativeSharp.Core;
class DerivedPayload : EscapingPayload { public DerivedPayload() { } }
class Program { public static int Main() { FAMILY<DerivedPayload> owner = SharedPointers.MAKE<DerivedPayload>();
 return 42; } }
''').replace('FAMILY', family).replace('MAKE', make), encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / ('binary-base-' + family + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', exe], root)
                output = result.stdout + result.stderr
                assert result.returncode != 0 and 'shared payload class has no verifiable precompiled lifecycle body' in output, (
                    'binary base accepted or failed for another reason', family, level, output)
                print('binary precompiled base rejected', family, level, flush=True)
        root_project = root / 'RootProducer.nsproj'
        root_project.write_text('<Project><PropertyGroup><Name>RootProducer</Name><Language>3</Language>'
                                '<OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup>'
                                '<Compile Include="RootProducer.ns" /></ItemGroup></Project>', encoding='utf-8')
        root_source = '''public class SafeRoot { public static SafeRoot Alias; public int Data;
 public SafeRoot() { Data = 42; } }
public class UnsafeRoot { public static UnsafeRoot Alias; public UnsafeRoot() { Alias = this; } }
public class UnknownRoot { public UnknownRoot() { Touch(); } public void Touch() { } }
'''
        (root / 'RootProducer.ns').write_text(root_source, encoding='utf-8')
        root_library = root / 'RootProducer.lib'
        result = run([compiler, '--emit-project-oop-lib', root_project, '-O0', '-o', root_library], root)
        assert result.returncode == 0, ('root producer', result.stdout + result.stderr)
        project.write_text('<Project><PropertyGroup><Name>BinarySharedConsumer</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" />'
                           '<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                           metadata.as_posix() + '" /><LibraryReference Include="' + root_library.as_posix() +
                           '" Metadata="' + root_library.with_suffix('.nsmeta').as_posix() +
                           '" /></ItemGroup></Project>', encoding='utf-8')
        for family, make in (('Rc', 'MakeRc'), ('Arc', 'MakeArc')):
            for payload in ('SafeRoot', 'UnsafeRoot', 'UnknownRoot'):
                source.write_text(('''using NativeSharp.Core;
class Program { public static int Main() { FAMILY<PAYLOAD> owner = SharedPointers.MAKE<PAYLOAD>(); BODY } }
''').replace('FAMILY', family).replace('MAKE', make).replace('PAYLOAD', payload).replace('BODY',
                    'return owner.Get().Data == 42 ? 42 : 1;' if payload == 'SafeRoot' else 'return 42;'),
                    encoding='utf-8')
                for level in ('O0', 'O2'):
                    exe = root / ('certified-root-' + family + '-' + payload + '-' + level + '.exe')
                    result = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', exe], root)
                    output = result.stdout + result.stderr
                    if payload == 'SafeRoot':
                        assert result.returncode == 0, ('certified root compile', family, level, output)
                        result = run([exe], root)
                        assert result.returncode == 42, ('certified root run', family, level, result.returncode)
                    else:
                        assert result.returncode != 0 and 'shared payload class has no verifiable precompiled lifecycle body' in output, (
                            'unproved root accepted or failed for another reason', family, payload, level, output)
                    print('binary root lifecycle', family, payload, level, 'verified', flush=True)
        derived_project = root / 'DerivedProducer.nsproj'
        derived_project.write_text('<Project><PropertyGroup><Name>DerivedProducer</Name><Language>3</Language>'
            '<OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup>'
            '<Compile Include="DerivedProducer.ns" /><LibraryReference Include="' + root_library.as_posix() +
            '" Metadata="' + root_library.with_suffix('.nsmeta').as_posix() +
            '" /></ItemGroup></Project>', encoding='utf-8')
        (root / 'DerivedProducer.ns').write_text(
            'public class DerivedSafe : SafeRoot { public DerivedSafe() { } }', encoding='utf-8')
        derived_library = root / 'DerivedProducer.lib'
        result = run([compiler, '--emit-project-oop-lib', derived_project, '-O0', '-o', derived_library], root)
        assert result.returncode == 0, ('derived producer', result.stdout + result.stderr)
        root_consumer_project = project.read_text(encoding='utf-8')
        project.write_text(root_consumer_project.replace('</ItemGroup>',
            '<LibraryReference Include="' + derived_library.as_posix() + '" Metadata="' +
            derived_library.with_suffix('.nsmeta').as_posix() + '" /></ItemGroup>'), encoding='utf-8')
        base_cache = '--artifact-cache=' + (root / 'base-lifecycle-cache').as_posix()
        for family, make in (('Rc', 'MakeRc'), ('Arc', 'MakeArc')):
            source.write_text(('''using NativeSharp.Core;
class Program { public static int Main() { FAMILY<DerivedSafe> owner = SharedPointers.MAKE<DerivedSafe>();
 return owner.Get().Data == 42 ? 42 : 1; } }
''').replace('FAMILY', family).replace('MAKE', make), encoding='utf-8')
            exe = root / ('derived-base-cached-' + family + '.exe')
            result = run([compiler, '--emit-project-oop-exe', project, '-O0', '-o', exe, base_cache], root)
            assert result.returncode == 0, ('certified binary base compile', family, result.stdout + result.stderr)
            result = run([exe], root)
            assert result.returncode == 42, ('certified binary base run', family, result.returncode)
        (root / 'RootProducer.ns').write_text(root_source.replace('Data = 42;', 'Data = 42; Alias = this;'),
                                             encoding='utf-8')
        result = run([compiler, '--emit-project-oop-lib', root_project, '-O0', '-o', root_library], root)
        assert result.returncode == 0, ('changed binary base producer', result.stdout + result.stderr)
        for family, make in (('Rc', 'MakeRc'), ('Arc', 'MakeArc')):
            source.write_text(('''using NativeSharp.Core;
class Program { public static int Main() { FAMILY<DerivedSafe> owner = SharedPointers.MAKE<DerivedSafe>();
 return owner.Get().Data == 42 ? 42 : 1; } }
''').replace('FAMILY', family).replace('MAKE', make), encoding='utf-8')
            exe = root / ('derived-base-cached-' + family + '.exe')
            result = run([compiler, '--emit-project-oop-exe', project, '-O0', '-o', exe, base_cache], root)
            output = result.stdout + result.stderr
            assert result.returncode != 0 and 'shared payload class has no verifiable precompiled lifecycle body' in output, (
                'cached consumer failed to observe changed binary base', family, output)
        print('binary derived lifecycle cache invalidated by base change', flush=True)
        (root / 'RootProducer.ns').write_text(root_source, encoding='utf-8')
        result = run([compiler, '--emit-project-oop-lib', root_project, '-O0', '-o', root_library], root)
        assert result.returncode == 0, ('restored root producer', result.stdout + result.stderr)
        project.write_text(root_consumer_project, encoding='utf-8')
        source.write_text('''using NativeSharp.Core;
class Program { public static int Main() { Arc<SafeRoot> owner = SharedPointers.MakeArc<SafeRoot>();
 return owner.Get().Data == 42 ? 42 : 1; } }
''', encoding='utf-8')
        cache_option = '--artifact-cache=' + (root / 'lifecycle-cache').as_posix()
        cached_exe = root / 'certified-root-cached.exe'
        result = run([compiler, '--emit-project-oop-exe', project, '-O0', '-o', cached_exe, cache_option], root)
        assert result.returncode == 0, ('certified root cached compile', result.stdout + result.stderr)
        result = run([cached_exe], root)
        assert result.returncode == 42, ('certified root cached run', result.returncode)
        (root / 'RootProducer.ns').write_text(root_source.replace('Data = 42;', 'Data = 42; Alias = this;'),
                                             encoding='utf-8')
        result = run([compiler, '--emit-project-oop-lib', root_project, '-O0', '-o', root_library], root)
        assert result.returncode == 0, ('changed root producer', result.stdout + result.stderr)
        result = run([compiler, '--emit-project-oop-exe', project, '-O0', '-o', cached_exe, cache_option], root)
        output = result.stdout + result.stderr
        assert result.returncode != 0 and 'shared payload class has no verifiable precompiled lifecycle body' in output, (
            'cached consumer failed to observe changed producer proof', output)
        print('binary lifecycle cache invalidated by producer change', flush=True)
        source_program = '''using NativeSharp.Core;
class Payload { public static Payload Alias; public int Data; public Payload() { BODY } }
class Program { public static int Main() { Arc<Payload> owner = SharedPointers.MakeArc<Payload>();
 return owner.Get().Data == 42 ? 42 : 1; } }
'''
        source.write_text(source_program.replace('BODY', 'Data = 42;'), encoding='utf-8')
        result = run([compiler, '--emit-project-oop-exe', project, '-O0', '-o', cached_exe, cache_option], root)
        assert result.returncode == 0, ('source payload cached compile', result.stdout + result.stderr)
        result = run([cached_exe], root)
        assert result.returncode == 42, ('source payload cached run', result.returncode)
        source.write_text(source_program.replace('BODY', 'Data = 42; Alias = this;'), encoding='utf-8')
        result = run([compiler, '--emit-project-oop-exe', project, '-O0', '-o', cached_exe, cache_option], root)
        output = result.stdout + result.stderr
        assert result.returncode != 0 and 'shared payload publishes its receiver' in output, (
            'cached consumer failed to observe changed source lifecycle', output)
        print('source lifecycle cache invalidated by constructor change', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
