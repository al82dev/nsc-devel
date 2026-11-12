import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(item) for item in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler, *sources = map(Path, sys.argv[1:])
    program = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } public trait Send {} public trait Sync {} }
class Resource { public static int Trace; private int id;
 public Resource(int value) { id = value; } ~Resource() { Trace = Trace * 10 + id; } }
class Base { public owned Resource baseField;
 public Base() { baseField = new Resource(1); }
 ~Base() { Resource.Trace = Resource.Trace * 10 + 2; } }
class FailsAfterBase : Base { public owned Resource derivedField;
 public FailsAfterBase() throws Exception { derivedField = new Resource(3); throw new Exception(); }
 ~FailsAfterBase() { Resource.Trace = Resource.Trace * 10 + 4; } }
class FailsFromCall : Base { public owned Resource derivedField;
 private static void Raise() throws Exception { throw new Exception(); }
 public FailsFromCall() throws Exception { derivedField = new Resource(3); Raise(); }
 ~FailsFromCall() { Resource.Trace = Resource.Trace * 10 + 4; } }
class FailsBeforeBase : Base {
 static FailsBeforeBase() throws Exception { throw new Exception(); }
 public FailsBeforeBase() throws Exception { throw new Exception(); }
 ~FailsBeforeBase() { Resource.Trace = Resource.Trace * 10 + 4; } }
class RecoversFromCall : Base { public owned Resource derivedField;
 private static void Raise() throws Exception { throw new Exception(); }
 public RecoversFromCall() throws Exception {
  try { Raise(); } catch (Exception problem) { } derivedField = new Resource(3); }
 ~RecoversFromCall() { Resource.Trace = Resource.Trace * 10 + 4; } }
class Successful : Base { public owned Resource derivedField;
 public Successful() throws Exception { derivedField = new Resource(3); }
 ~Successful() { Resource.Trace = Resource.Trace * 10 + 4; } }
class Recovers : Base { public owned Resource derivedField;
 public Recovers() throws Exception { try { throw new Exception(); } catch (Exception problem) { }
 derivedField = new Resource(3); }
 ~Recovers() { Resource.Trace = Resource.Trace * 10 + 4; } }
class Middle : Base { public owned Resource middleField;
 public Middle() { middleField = new Resource(4); }
 ~Middle() { Resource.Trace = Resource.Trace * 10 + 3; } }
class ImplicitMiddle : Base { public owned Resource middleField = new Resource(4);
 ~ImplicitMiddle() { Resource.Trace = Resource.Trace * 10 + 3; } }
class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
class FailingImplicitMiddle : Base { public owned Resource middleField = new Resource(4);
 public int marker = Gate.Read();
 ~FailingImplicitMiddle() { Resource.Trace = Resource.Trace * 10 + 3; } }
class FailsDuringImplicitBase : FailingImplicitMiddle {
 public FailsDuringImplicitBase() throws Exception { }
 ~FailsDuringImplicitBase() { Resource.Trace = Resource.Trace * 10 + 6; } }
class FailsAfterImplicitBase : ImplicitMiddle { public owned Resource derivedField;
 public FailsAfterImplicitBase() throws Exception { derivedField = new Resource(5); throw new Exception(); }
 ~FailsAfterImplicitBase() { Resource.Trace = Resource.Trace * 10 + 6; } }
class FailsAfterTwoBases : Middle { public owned Resource derivedField;
 public FailsAfterTwoBases() throws Exception { derivedField = new Resource(5); throw new Exception(); }
 ~FailsAfterTwoBases() { Resource.Trace = Resource.Trace * 10 + 6; } }
class DelegatingFailure : Base { public owned Resource derivedField;
 private DelegatingFailure(int marker) throws Exception { derivedField = new Resource(3); throw new Exception(); }
 public DelegatingFailure() throws Exception : this(0) { }
 ~DelegatingFailure() { Resource.Trace = Resource.Trace * 10 + 4; } }
class FailingBase { public owned Resource baseField;
 public FailingBase() throws Exception { baseField = new Resource(1); throw new Exception(); }
 ~FailingBase() { Resource.Trace = Resource.Trace * 10 + 2; } }
class FailsInBase : FailingBase { public owned Resource derivedField;
 public FailsInBase() throws Exception { derivedField = new Resource(3); }
 ~FailsInBase() { Resource.Trace = Resource.Trace * 10 + 4; } }
class HelperFailingBase { public owned Resource baseField;
 private void Initialize() { baseField = new Resource(1); }
 public HelperFailingBase() throws Exception { Initialize(); throw new Exception(); }
 ~HelperFailingBase() { Resource.Trace = Resource.Trace * 10 + 2; } }
class FailsInBaseHelper : HelperFailingBase { public FailsInBaseHelper() throws Exception { } }
class FailsAfterBaseHelper : Base { public owned Resource derivedField;
 private void Initialize() { derivedField = new Resource(3); }
 public FailsAfterBaseHelper() throws Exception { Initialize(); throw new Exception(); } }
class HelperThrowsBase { public owned Resource baseField;
 private void Initialize() throws Exception { baseField = new Resource(1); throw new Exception(); }
 public HelperThrowsBase() throws Exception { Initialize(); }
 ~HelperThrowsBase() { Resource.Trace = Resource.Trace * 10 + 2; } }
class FailsDuringBaseHelper : HelperThrowsBase { public FailsDuringBaseHelper() throws Exception { } }
class FailsDuringDerivedHelper : Base { public owned Resource derivedField;
 private void Initialize() throws Exception { derivedField = new Resource(3); throw new Exception(); }
 public FailsDuringDerivedHelper() throws Exception { Initialize(); } }
class Program { public static int Main() {
 int caught = 0; Resource.Trace = 0;
 try { STRONG<FailsAfterBase> owner = SharedPointers.MAKE_THROWING<FailsAfterBase>(); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 1 || Resource.Trace != 321) return 1;
 Resource.Trace = 0;
 try { STRONG<FailsFromCall> owner = SharedPointers.MAKE_THROWING<FailsFromCall>(); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 2 || Resource.Trace != 321) return 10;
 Resource.Trace = 0;
 try { STRONG<FailsBeforeBase> owner = SharedPointers.MAKE_THROWING<FailsBeforeBase>(); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 3 || Resource.Trace != 0) return 13;
 Resource.Trace = 0;
 try { STRONG<RecoversFromCall> owner = SharedPointers.MAKE_THROWING<RecoversFromCall>(); }
 catch (Exception error) { return 11; }
 if (Resource.Trace != 4321) return 12;
 Resource.Trace = 0;
 try { STRONG<FailsInBase> owner = SharedPointers.MAKE_THROWING<FailsInBase>(); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 4 || Resource.Trace != 1) return 2;
 Resource.Trace = 0;
 try { STRONG<Successful> owner = SharedPointers.MAKE_THROWING<Successful>(); }
 catch (Exception error) { return 3; }
 if (Resource.Trace != 4321) return 4;
 Resource.Trace = 0;
 try { STRONG<Recovers> owner = SharedPointers.MAKE_THROWING<Recovers>(); }
 catch (Exception error) { return 5; }
 if (Resource.Trace != 4321) return 6;
 Resource.Trace = 0;
 try { STRONG<FailsAfterTwoBases> owner = SharedPointers.MAKE_THROWING<FailsAfterTwoBases>(); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 5 || Resource.Trace != 53421) return 7;
 Resource.Trace = 0;
 try { STRONG<DelegatingFailure> owner = SharedPointers.MAKE_THROWING<DelegatingFailure>(); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 6 || Resource.Trace != 321) return 8;
 Resource.Trace = 0;
 try { STRONG<FailsAfterImplicitBase> owner = SharedPointers.MAKE_THROWING<FailsAfterImplicitBase>(); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 7 || Resource.Trace != 53421) return 9;
 Resource.Trace = 0;
 try { STRONG<FailsDuringImplicitBase> owner = SharedPointers.MAKE_THROWING<FailsDuringImplicitBase>(); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 8 || Resource.Trace != 421) return 14;
 Resource.Trace = 0;
 try { STRONG<FailsInBaseHelper> owner = SharedPointers.MAKE_THROWING<FailsInBaseHelper>(); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 9 || Resource.Trace != 1) return 15;
 Resource.Trace = 0;
 try { STRONG<FailsAfterBaseHelper> owner = SharedPointers.MAKE_THROWING<FailsAfterBaseHelper>(); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 10 || Resource.Trace != 321) return 16;
 Resource.Trace = 0;
 try { STRONG<FailsDuringBaseHelper> owner = SharedPointers.MAKE_THROWING<FailsDuringBaseHelper>(); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 11 || Resource.Trace != 1) return 17;
 Resource.Trace = 0;
 try { STRONG<FailsDuringDerivedHelper> owner = SharedPointers.MAKE_THROWING<FailsDuringDerivedHelper>(); }
 catch (Exception error) { caught = caught + 1; }
 return caught == 12 && Resource.Trace == 321 ? 42 : 18;
} }
'''
    with tempfile.TemporaryDirectory(prefix='nsc-shared-throwing-base-') as temporary:
        root = Path(temporary)
        library = os.environ.get('NSC_SHARED_BINARY_SDK')
        if library:
            path = Path(library).resolve()
            metadata = path.with_suffix('.nsmeta')
            assert path.is_file() and metadata.is_file()
            references = ('<LibraryReference Include="' + path.as_posix() + '" Metadata="' +
                          metadata.as_posix() + '" />')
            program = program.replace('namespace NativeSharp.Core { public class Exception { } public trait Send {} public trait Sync {} }\n', '')
        else:
            for source in sources:
                shutil.copyfile(source, root / source.name)
            references = ''
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" />' + references +
                           '</ItemGroup></Project>', encoding='utf-8')
        selected = os.environ.get('NSC_SHARED_BASE_FAMILY')
        families = (selected,) if selected else ('Rc', 'Arc')
        if any(strong not in ('Rc', 'Arc') for strong in families):
            raise ValueError('unknown shared pointer family')
        for strong in families:
            source = program.replace('STRONG', strong).replace('MAKE_THROWING', 'Make' + strong + 'Throwing')
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', project], root)
            assert result.returncode == 0, (strong, 'semantic', result.stdout + result.stderr)
            print(strong, 'semantic check passed', flush=True)
            for level in ('O0', 'O2'):
                exe = root / (strong + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-o', exe, '-' + level,
                              '--no-artifact-cache'], root)
                assert result.returncode == 0, (strong, level, result.stdout + result.stderr)
                if os.name == 'nt':
                    result = run([exe], root)
                    assert result.returncode == 42, (strong, level, result.returncode, result.stdout, result.stderr)
                    print(strong, level, 'exit=42', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
