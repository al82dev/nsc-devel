"""Regression proposal for shared factories and an imported NativeSharp base."""
from html import escape
from pathlib import Path
import subprocess
import sys
from isolated_a_runner import absolute_a, setup

REPOSITORY = Path(__file__).resolve().parents[2]

PRODUCER = '''using NativeSharp.Core;
public class Root {
 public static int Drops;
 public int number;
 public Root() { number = 7; }
 ~Root() { Drops = Drops + 1; }
}
'''
CONSUMER = '''using NativeSharp.Core;
class Payload { public static int Drops; public Payload() { }
 ~Payload() { Drops = Drops + 1; } }
class Child : Root { public static int Drops; public Rc<Payload> strong;
 public WeakRc<Payload> weak; public Arc<Payload> arc; public WeakArc<Payload> weakArc;
 public Child() throws Exception : this(1) { }
 public Child(int marker) throws Exception {
  strong = SharedPointers.MakeRc<Payload>(); weak = strong.Downgrade();
  arc = SharedPointers.MakeArc<Payload>(); weakArc = arc.Downgrade();
  if (MODE == 1) throw new Exception();
 }
 ~Child() { Drops = Drops + 1; }
}
class Program { public static int Main() {
 int caught = 0;
 try { HANDLE<Child> value = SharedPointers.FACTORY<Child>(); }
 catch (Exception error) { caught = caught + 1; }
 return caught == MODE && Root.Drops == 1 && Payload.Drops == 2 &&
        Child.Drops == EXPECTED ? 42 : 1;
} }
'''

def reference(library, metadata):
    return ('<LibraryReference Include="' + escape(library.as_posix(), quote=True) +
            '" Metadata="' + escape(metadata.as_posix(), quote=True) + '"/>')

def run(command):
    command = [str(item) for item in command]
    result = subprocess.run(command, cwd=REPOSITORY, env=ENVIRONMENT, capture_output=True, text=True,
                            errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result.returncode

def main():
    if len(sys.argv) != 5:
        raise SystemExit('expected nsc, Core.lib, Core.nsmeta and new evidence directory')
    compiler, core, metadata, root = (absolute_a(value) for value in sys.argv[1:])
    if any(value.drive.lower() != 'a:' for value in (compiler, core, metadata, root)):
        raise SystemExit('compiler, SDK, metadata and evidence must be isolated on A:')
    global ENVIRONMENT
    ENVIRONMENT = setup(root, REPOSITORY)
    (root / 'Producer.ns').write_text(PRODUCER, encoding='utf-8')
    (root / 'Producer-base.ns').write_text(PRODUCER, encoding='utf-8')
    producer = root / 'Producer.nsproj'
    producer.write_text('<Project><PropertyGroup><Name>ImportedRoot</Name>'
        '<OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup>'
        '<Compile Include="Producer.ns"/>' + reference(core, metadata) +
        '</ItemGroup></Project>', encoding='utf-8')
    failures = []
    for producer_level in ('O0', 'O2'):
        library = root / ('Root-' + producer_level + '.lib')
        if run([compiler, '--emit-project-oop-lib', producer, '-' + producer_level,
                '-o', library, '--no-artifact-cache']):
            failures.append((producer_level, 'producer'))
            continue
        project = root / 'Consumer.nsproj'
        project.write_text('<Project><PropertyGroup><Name>ImportedRootConsumer</Name></PropertyGroup>'
            '<ItemGroup><Compile Include="Main.ns"/>' + reference(core, metadata) +
            reference(library, library.with_suffix('.nsmeta')) + '</ItemGroup></Project>',
            encoding='utf-8')
        for handle, factory in (('Rc', 'MakeRcThrowing'), ('Arc', 'MakeArcThrowing')):
            for mode in (0, 1):
                source = CONSUMER.replace('HANDLE', handle).replace('FACTORY', factory)
                source = source.replace('MODE', str(mode)).replace('EXPECTED', str(1 - mode))
                (root / (handle + '-' + str(mode) + '-' + producer_level + '.ns')).write_text(
                    source, encoding='utf-8')
                (root / 'Main.ns').write_text(source, encoding='utf-8')
                for level in ('O0', 'O2'):
                    name = handle + '-' + str(mode) + '-' + producer_level + '-' + level
                    executable = root / (name + '.exe')
                    if run([compiler, '--emit-project-oop-exe', project, '-' + level,
                            '-o', executable, '--no-artifact-cache']):
                        failures.append((name, 'consumer'))
                    elif run([executable]) != 42:
                        failures.append((name, 'execution'))
    # A binary constructor whose effect permits throwing must remain rejected:
    # non-escape metadata does not prove partial-construction cleanup.
    (root / 'Producer.ns').write_text(
        PRODUCER.replace('public Root()', 'public Root() throws Exception'), encoding='utf-8')
    (root / 'Producer-throws.ns').write_text(
        PRODUCER.replace('public Root()', 'public Root() throws Exception'), encoding='utf-8')
    library = root / 'Root-throws.lib'
    if run([compiler, '--emit-project-oop-lib', producer, '-O0', '-o', library,
            '--no-artifact-cache']):
        failures.append(('throwing-base', 'producer'))
    else:
        project = root / 'Consumer.nsproj'
        project.write_text('<Project><PropertyGroup><Name>ImportedRootConsumer</Name></PropertyGroup>'
            '<ItemGroup><Compile Include="Main.ns"/>' + reference(core, metadata) +
            reference(library, library.with_suffix('.nsmeta')) + '</ItemGroup></Project>',
            encoding='utf-8')
        source = CONSUMER.replace('HANDLE', 'Rc').replace('FACTORY', 'MakeRcThrowing')
        source = source.replace('MODE', '1').replace('EXPECTED', '0')
        (root / 'Main.ns').write_text(source, encoding='utf-8')
        (root / 'Main-throws.ns').write_text(source, encoding='utf-8')
        command = [str(compiler), '--emit-project-oop-exe', str(project), '-O0',
                   '-o', str(root / 'reject.exe'), '--no-artifact-cache']
        result = subprocess.run(command, cwd=REPOSITORY, env=ENVIRONMENT, capture_output=True, text=True,
                                errors='replace', timeout=120)
        print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
              result.stdout + result.stderr, flush=True)
        if result.returncode == 0 or 'shared throwing construction cannot safely clean this payload shape' not in (result.stdout + result.stderr):
            failures.append(('throwing-base', 'expected-guard'))
    print('FAILURES', failures, flush=True)
    return bool(failures)

if __name__ == '__main__':
    sys.exit(main())
