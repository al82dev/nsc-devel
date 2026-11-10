"""Directed contract checks for shared pointer thread-safety constraints."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1]).resolve()
    options = sys.argv[2:]
    prototype = '--prototype-contract' in options
    arguments = [value for value in options if value != '--prototype-contract']
    library = Path(arguments[0]).resolve() if arguments else None
    if prototype and library is not None:
        raise ValueError('prototype contracts require the isolated source fixture')
    sdk = Path(__file__).resolve().parents[2] / 'lib' / 'NativeSharp.Core'
    names = ('SharedStorage', 'SharedControlStorage', 'SharedReferenceCounts', 'SharedLocalControl',
             'SharedAtomicControl', 'SharedBorrow', 'SharedLocalHandle', 'SharedLocalWeak',
             'SharedLocalFactory', 'SharedAtomicHandle', 'SharedAtomicWeak', 'SharedAtomicFactory',
             'SharedView', 'Rc', 'WeakRc', 'Arc', 'WeakArc', 'SharedPointers')
    preamble = '''using NativeSharp.Core;
struct Safe { public int value; }
struct Raw { public int* value; }
struct SendOnly { public ref mut int value; }
class Program { public static int Require<T>() where T : MARKER { return 42; }
 public static int Main() { return Require<TARGET>(); } }
'''
    cases = {
        'Safe': (True, True), 'Raw': (False, False), 'SendOnly': (True, False),
        'Rc<Safe>': (False, False), 'WeakRc<Safe>': (False, False),
        'Arc<Safe>': (True, True), 'WeakArc<Safe>': (True, True),
        'Arc<Arc<Safe>>': (True, True), 'Arc<WeakArc<Safe>>': (True, True),
        'Arc<Rc<Safe>>': (False, False), 'Arc<Arc<Raw>>': (False, False),
        'Arc<Raw>': (False, False), 'WeakArc<Raw>': (False, False),
        'Arc<SendOnly>': (False, False), 'WeakArc<SendOnly>': (False, False),
    }
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-arc-thread-constraints-') as temporary:
        root = Path(temporary)
        if library is None:
            for name in names:
                shutil.copyfile(sdk / (name + '.ns'), root / (name + '.ns'))
            if prototype:
                for name in ('Arc', 'WeakArc'):
                    with (root / (name + '.ns')).open('a', encoding='utf-8') as stream:
                        stream.write('\nnamespace NativeSharp.Core {\n'
                                     'unsafe impl<T> Send for ' + name + '<T> where T : Send, Sync {}\n'
                                     'unsafe impl<T> Sync for ' + name + '<T> where T : Send, Sync {}\n}\n')
            exception = ('namespace NativeSharp.Core { public class Exception { } }\n'
                         'trait Send {} trait Sync {}\n')
            references = ''
        else:
            metadata = library.with_suffix('.nsmeta')
            assert library.is_file() and metadata.is_file()
            exception = ''
            references = ('<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                          metadata.as_posix() + '" />')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name>'
                           '</PropertyGroup><ItemGroup><Compile Include="*.ns" />' + references +
                           '</ItemGroup></Project>', encoding='utf-8')
        for target, expected in cases.items():
            for index, marker in enumerate(('Send', 'Sync')):
                source = exception + preamble.replace('MARKER', marker).replace('TARGET', target)
                (root / 'Main.ns').write_text(source, encoding='utf-8')
                command = [str(compiler), '--check-project-method-bodies', str(project)]
                result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                output = result.stdout + result.stderr
                accepted = result.returncode == 0
                correct = accepted == expected[index] and (accepted or 'requires an implementation of' in output)
                print(marker, target, 'accepted' if accepted else 'rejected',
                      'expected=' + ('accept' if expected[index] else 'reject'), flush=True)
                if not correct:
                    failures.append((marker, target, result.returncode, output))
        fake_root = root / 'homonymous'
        fake_root.mkdir()
        fake_project = fake_root / 'Fake.nsproj'
        fake_project.write_text('<Project><PropertyGroup><Name>NativeSharpCore</Name>'
                                '</PropertyGroup>'
                                '<ItemGroup><Compile Include="Fake.ns" /></ItemGroup></Project>',
                                encoding='utf-8')
        fake_source = '''using NativeSharp.Core;
namespace NativeSharp.Core {
 public struct Arc<T> { public int* raw; }
 public struct WeakArc<T> { public int* raw; }
}
trait Send {} trait Sync {} struct Safe { public int value; }
class Program { public static int Require<T>() where T : MARKER { return 42; }
 public static int Main() { return Require<TARGET<Safe>>(); } }
'''
        for target in ('Arc', 'WeakArc'):
            for marker in ('Send', 'Sync'):
                (fake_root / 'Fake.ns').write_text(
                    fake_source.replace('MARKER', marker).replace('TARGET', target), encoding='utf-8')
                command = [str(compiler), '--check-project-method-bodies', str(fake_project)]
                result = subprocess.run(command, cwd=fake_root, capture_output=True, text=True,
                                        errors='replace', timeout=120)
                output = result.stdout + result.stderr
                print(marker, 'homonymous:' + target, 'accepted' if result.returncode == 0 else 'rejected',
                      'expected=reject', flush=True)
                if result.returncode == 0 or 'requires an implementation of' not in output:
                    failures.append((marker, 'homonymous:' + target, result.returncode, output))
        # The payload must be in Probe, as with the real SDK: this makes the
        # canonical specialization key identical to the actual Arc<Safe>.
        producer = root / 'counterfeit-producer'
        consumer = root / 'counterfeit-consumer'
        producer.mkdir()
        consumer.mkdir()
        (producer / 'Fake.nsproj').write_text(
            '<Project><PropertyGroup><Name>NativeSharpCore</Name>'
            '</PropertyGroup><ItemGroup><Compile Include="Fake.ns" /></ItemGroup></Project>',
            encoding='utf-8')
        (producer / 'Fake.ns').write_text(
            'namespace NativeSharp.Core { public struct Arc<T> { public int* raw; } '
            'public struct WeakArc<T> { public int* raw; } }', encoding='utf-8')
        counterfeit_project = consumer / 'Probe.nsproj'
        counterfeit_project.write_text(
            '<Project><PropertyGroup><Name>Probe</Name>'
            '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
            '<ProjectReference Include="../counterfeit-producer/Fake.nsproj" />'
            '</ItemGroup></Project>', encoding='utf-8')
        counterfeit_source = '''using NativeSharp.Core;
trait Send {} trait Sync {} struct Safe { public int value; }
class Program { public static int Require<T>() where T : MARKER { return 42; }
 public static int Main() { return Require<TARGET<Safe>>(); } }
'''
        for target in ('Arc', 'WeakArc'):
            for marker in ('Send', 'Sync'):
                (consumer / 'Main.ns').write_text(
                    counterfeit_source.replace('MARKER', marker).replace('TARGET', target),
                    encoding='utf-8')
                command = [str(compiler), '--check-project-method-bodies', str(counterfeit_project)]
                result = subprocess.run(command, cwd=consumer, capture_output=True, text=True,
                                        errors='replace', timeout=120)
                output = result.stdout + result.stderr
                print(marker, 'counterfeit:' + target, 'accepted' if result.returncode == 0 else 'rejected',
                      'expected=reject', flush=True)
                if result.returncode == 0 or 'requires an implementation of' not in output:
                    failures.append((marker, 'counterfeit:' + target, result.returncode, output))
        # Repeat with the counterfeit producer serialized to a binary library.
        # Its restored generic specializations have the same canonical keys as
        # the real library, so precompiled status alone is not a trust marker.
        (producer / 'Fake.nsproj').write_text(
            '<Project><PropertyGroup><Name>NativeSharpCore</Name>'
            '<OutputType>Library</OutputType></PropertyGroup><ItemGroup>'
            '<Compile Include="Fake.ns" /></ItemGroup></Project>', encoding='utf-8')
        fake_library = root / 'NativeSharpCore.lib'
        command = [str(compiler), '--emit-project-oop-lib', str(producer / 'Fake.nsproj'),
                   '-O0', '-o', str(fake_library)]
        built = subprocess.run(command, cwd=producer, capture_output=True, text=True,
                               errors='replace', timeout=120)
        if built.returncode != 0:
            failures.append(('build', 'binary-counterfeit', built.returncode, built.stdout + built.stderr))
        else:
            counterfeit_project.write_text(
                '<Project><PropertyGroup><Name>Probe</Name>'
                '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                '<LibraryReference Include="' + fake_library.as_posix() + '" Metadata="' +
                fake_library.with_suffix('.nsmeta').as_posix() + '" />'
                '</ItemGroup></Project>', encoding='utf-8')
            for target in ('Arc', 'WeakArc'):
                for marker in ('Send', 'Sync'):
                    (consumer / 'Main.ns').write_text(
                        counterfeit_source.replace('MARKER', marker).replace('TARGET', target),
                        encoding='utf-8')
                    command = [str(compiler), '--check-project-method-bodies', str(counterfeit_project)]
                    result = subprocess.run(command, cwd=consumer, capture_output=True, text=True,
                                            errors='replace', timeout=120)
                    output = result.stdout + result.stderr
                    print(marker, 'binary-counterfeit:' + target,
                          'accepted' if result.returncode == 0 else 'rejected',
                          'expected=reject', flush=True)
                    if result.returncode == 0 or 'requires an implementation of' not in output:
                        failures.append((marker, 'binary-counterfeit:' + target, result.returncode, output))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
