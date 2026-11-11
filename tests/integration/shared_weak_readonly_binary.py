"""Check readonly weak observations from a separately built Core subset."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


NAMES = ('SharedStorage', 'SharedControlStorage', 'SharedReferenceCounts', 'SharedLocalControl',
         'SharedAtomicControl', 'SharedBorrow', 'SharedLocalHandle', 'SharedLocalWeak',
         'SharedLocalFactory', 'SharedAtomicHandle', 'SharedAtomicWeak', 'SharedAtomicFactory',
         'SharedView', 'Rc', 'WeakRc', 'Arc', 'WeakArc', 'SharedPointers')


def run(command, cwd):
    result = subprocess.run([str(item) for item in command], cwd=cwd, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline([str(item) for item in command]),
          'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler, core_sources = map(lambda value: Path(value).resolve(), sys.argv[1:3])
    with tempfile.TemporaryDirectory(prefix='nsc-weak-readonly-binary-') as temporary:
        root = Path(temporary)
        producer = root / 'producer'
        consumer = root / 'consumer'
        producer.mkdir()
        consumer.mkdir()
        for name in NAMES:
            shutil.copyfile(core_sources / (name + '.ns'), producer / (name + '.ns'))
        (producer / 'Contracts.ns').write_text(
            'namespace NativeSharp.Core { public class Exception { } public trait Send {} public trait Sync {} }',
            encoding='utf-8')
        project = producer / 'Core.nsproj'
        project.write_text('<Project><PropertyGroup><Name>NativeSharpCore</Name>'
                           '<OutputType>StaticLibrary</OutputType></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns"/></ItemGroup></Project>', encoding='utf-8')
        library = producer / 'NativeSharpCore.lib'
        result = run([compiler, '--emit-project-oop-lib', project, '-O0', '-o', library,
                      '--artifact-cache=' + str(root / 'cache')], producer)
        if result.returncode:
            return 1
        consumer_project = consumer / 'Probe.nsproj'
        consumer_project.write_text('<Project><PropertyGroup><Name>WeakConsumer</Name></PropertyGroup>'
            '<ItemGroup><Compile Include="Main.ns"/><LibraryReference Include="' + library.as_posix() +
            '" Metadata="' + library.with_suffix('.nsmeta').as_posix() + '"/></ItemGroup></Project>',
            encoding='utf-8')
        for strong, weak in (('Rc', 'WeakRc'), ('Arc', 'WeakArc')):
            (consumer / 'Main.ns').write_text(('''using NativeSharp.Core;
public class Payload { public Payload() { } }
class Program { static int Observe(in WEAK<Payload> weak) {
 int expired = weak.Expired() ? 100 : 0; return expired + (int)weak.WeakCount(); }
 public static int Main() { WEAK<Payload> weak = default(WEAK<Payload>);
 if (Observe(in weak) != 100) return 1;
 { STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); weak = owner.Downgrade();
   if (Observe(in weak) != 1) return 2; }
 return Observe(in weak) == 101 && weak.WeakCount() == 1l ? 42 : 3; } }'''
                .replace('STRONG', strong).replace('WEAK', weak).replace('MAKE', 'Make' + strong)),
                encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = consumer / (strong + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', consumer_project, '-' + level,
                              '-o', executable], consumer)
                if result.returncode or run([executable], consumer).returncode != 42:
                    return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
