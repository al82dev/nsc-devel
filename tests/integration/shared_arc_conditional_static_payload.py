"""Shared-pointer lifecycle checks with a payload from a separate library."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(value) for value in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler = Path(sys.argv[1]).resolve()
    core = Path(sys.argv[2]).resolve()
    options = sys.argv[3:]
    if any(option != '--dll' for option in options) or options.count('--dll') > 1:
        raise ValueError('expected only the optional --dll argument')
    dynamic = '--dll' in options
    with tempfile.TemporaryDirectory(prefix='nsc-arc-static-payload-') as temporary:
        root = Path(temporary)
        producer = root / 'producer'
        consumer = root / 'consumer'
        producer.mkdir()
        consumer.mkdir()
        (producer / 'Payload.ns').write_text('''public class SafePlain { public int Value;
 public SafePlain() { Value = 42; } }
public class SafeEmpty { public int Value; public SafeEmpty() { Value = 42; } ~SafeEmpty() { } }
public class SafeField { public int Value; public SafeField() { Value = 42; } ~SafeField() { Value = 0; } }
public class SafePayload {
 public static int Drops; public int Value; public SafePayload() { Value = 42; }
 ~SafePayload() { Drops = Drops + 1; } }
public class EscapingPayload { public static EscapingPayload Alias;
 public EscapingPayload() { Alias = this; } }
public class EscapingDestructor { public static EscapingDestructor Alias;
 public EscapingDestructor() { } ~EscapingDestructor() { Alias = this; } }
public class EscapingMixed { public static EscapingMixed Alias; public static int Drops;
 public EscapingMixed() { } ~EscapingMixed() { Drops = Drops + 1; Alias = this; } }
''', encoding='utf-8')
        producer_project = producer / 'Producer.nsproj'
        producer_project.write_text('<Project><PropertyGroup><Name>PayloadProducer</Name><Language>3</Language>'
                                    '<OutputType>' + ('SharedLibrary' if dynamic else 'Library') +
                                    '</OutputType></PropertyGroup><ItemGroup>'
                                    '<Compile Include="Payload.ns" /></ItemGroup></Project>', encoding='utf-8')
        payload_lib = producer / 'PayloadProducer.lib'
        output = producer / 'PayloadProducer.dll' if dynamic else payload_lib
        result = run([compiler, '--emit-project-oop-dll' if dynamic else '--emit-project-oop-lib',
                      producer_project, '-O0', '-o', output,
                      '--no-artifact-cache'], producer)
        print('producer compile=' + str(result.returncode), flush=True)
        if result.returncode != 0:
            print(result.stdout + result.stderr, file=sys.stderr)
            return 1
        if dynamic:
            shutil.copy2(output, consumer)
        project = consumer / 'Consumer.nsproj'
        def reference(library):
            return ('<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                    library.with_suffix('.nsmeta').as_posix() + '" />')
        project.write_text('<Project><PropertyGroup><Name>PayloadConsumer</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />' + reference(core) +
                           reference(payload_lib) + '</ItemGroup></Project>', encoding='utf-8')
        main_source = consumer / 'Main.ns'
        valid = '''using NativeSharp.Core;
class Program { public static int Main() { WeakArc<SafePayload> weak;
 { Arc<SafePayload> owner = SharedPointers.MakeArc<SafePayload>(); weak = owner.Downgrade();
   if (owner.Get().Value != 42 || SafePayload.Drops != 0) return 1; }
 return weak.Expired() && SafePayload.Drops == 1 ? 42 : 2; } }
'''
        plain = '''using NativeSharp.Core;
class Program { public static int Main() { Arc<PAYLOAD> owner = SharedPointers.MakeArc<PAYLOAD>();
 return owner.Get().Value == 42 ? 42 : 1; } }
'''
        local = valid.replace('using NativeSharp.Core;', '''using NativeSharp.Core;
class LocalPayload { public static int Drops; public int Value; public LocalPayload() { Value = 42; }
 ~LocalPayload() { Drops = Drops + 1; } }''').replace('SafePayload', 'LocalPayload')
        valid_rc = valid.replace('Arc', 'Rc')
        invalid = '''using NativeSharp.Core;
class Program { public static int Main() {
 Arc<EscapingPayload> owner = SharedPointers.MakeArc<EscapingPayload>(); return 42; } }
'''
        failures = []
        for level in ('O0', 'O2'):
            for name, source in (('plain', plain.replace('PAYLOAD', 'SafePlain')),
                                 ('empty-destructor', plain.replace('PAYLOAD', 'SafeEmpty')),
                                 ('field-destructor', plain.replace('PAYLOAD', 'SafeField')),
                                 ('local-counting-destructor', local),
                                 ('counting-destructor-arc', valid),
                                 ('counting-destructor-rc', valid_rc)):
                executable = consumer / (name + '-' + level + '.exe')
                main_source.write_text(source, encoding='utf-8')
                result = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', executable,
                              '--no-artifact-cache'], consumer)
                print(name, level, 'compile=' + str(result.returncode), flush=True)
                if result.returncode != 0:
                    failures.append((name, level, result.stdout + result.stderr))
                    continue
                result = run([executable], consumer)
                print(name, level, 'run=' + str(result.returncode), flush=True)
                if result.returncode != 42:
                    failures.append((name, level, result.stdout + result.stderr))
            for family in ('Arc', 'Rc'):
                for name, payload in (('constructor', 'EscapingPayload'),
                                      ('destructor', 'EscapingDestructor'),
                                      ('counter-and-destructor', 'EscapingMixed')):
                    main_source.write_text(invalid.replace('Arc', family).replace('EscapingPayload', payload),
                                           encoding='utf-8')
                    result = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                                  '-o', consumer / ('invalid-' + family + '-' + name + '-' + level + '.exe'),
                                  '--no-artifact-cache'], consumer)
                    output = result.stdout + result.stderr
                    print('escaping-' + name, family, level, 'rejected=' + str(result.returncode != 0), flush=True)
                    if result.returncode == 0 or 'shared payload class has no verifiable precompiled lifecycle body' not in output:
                        failures.append(('escaping-' + name, family, level, output))
        for failure in failures:
            print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
