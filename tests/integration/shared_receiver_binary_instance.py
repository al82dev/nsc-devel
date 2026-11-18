"""Check unverified binary instance calls on a shared receiver; never execute consumers."""
from pathlib import Path
from xml.sax.saxutils import escape
import hashlib
import os
import subprocess
import sys
import tempfile


def main():
    repo = Path(__file__).resolve().parents[2]
    compiler = Path(sys.argv[1]).resolve()
    core_name = os.environ.get('NSC_TEST_CORE_BINARY')
    temp_name = os.environ.get('NSC_TEST_TEMP_ROOT') or os.environ.get('TEMP')
    if not core_name:
        print('SKIP: NSC_TEST_CORE_BINARY must name an isolated Core library with metadata')
        return 77
    if not Path(core_name).is_file() or not Path(core_name).with_suffix('.nsmeta').is_file():
        print('FAIL: configured Core library or metadata is missing', file=sys.stderr)
        return 1
    if not temp_name or not Path(temp_name).is_dir() or Path(temp_name).resolve().drive.upper() != 'A:':
        print('SKIP: NSC_TEST_TEMP_ROOT or TEMP must name an existing directory on A:')
        return 77
    core = Path(core_name).resolve()
    def hashes():
        return {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in (compiler, core, core.with_suffix('.nsmeta'))}
    before = hashes()
    print('SHA256 BEFORE', before, flush=True)
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-shared-receiver-instance-', dir=temp_name) as temporary:
        root = Path(temporary)
        env = os.environ.copy()
        for key in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'): env[key] = str(root)
        def attr(path): return escape(path.as_posix(), {'"': '&quot;'})
        def reference(path):
            return '<LibraryReference Include="' + attr(path) + '" Metadata="' + attr(path.with_suffix('.nsmeta')) + '"/>'
        def project(name, source, refs, output):
            return ('<Project><PropertyGroup><Name>' + name + '</Name><OutputType>' + output +
                    '</OutputType></PropertyGroup><ItemGroup><Compile Include="' + source + '"/>' +
                    ''.join(reference(x) for x in refs) + '</ItemGroup></Project>')
        def invoke(command):
            command = [str(x) for x in command]
            result = subprocess.run(command, cwd=repo, env=env, capture_output=True,
                                    text=True, errors='replace', timeout=120)
            print('CWD', repo, 'COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                  result.stdout + result.stderr, flush=True)
            return result
        producer = root / 'Producer.nsproj'
        (root / 'Producer.ns').write_text('''public class Base { public static Base Alias; public Base() { }
 public void Publish() { Alias = this; } public void Safe() { } public void Safe(int value) { } }
public class UnsafeBase { public static UnsafeBase Alias; public UnsafeBase() { Alias = this; } }''', encoding='utf-8')
        producer.write_text(project('ReceiverInstanceProducer', 'Producer.ns', [], 'StaticLibrary'), encoding='utf-8')
        library = root / 'producer.lib'
        built = invoke([compiler, '--emit-project-oop-lib', producer, '-O0', '-o', library, '--no-artifact-cache'])
        if built.returncode != 0 or not library.is_file() or not library.with_suffix('.nsmeta').is_file():
            failures.append(('producer-or-metadata',))
        else:
            cases = (('binary-publishes', 'base.Publish();', 'unverifiable'),
                     ('binary-safe', 'base.Safe();', 'unverifiable'),
                     ('binary-safe-overload', 'base.Safe(7);', 'unverifiable'),
                     ('source-publishes', 'this.LocalPublish();', 'publishes'),
                     ('source-safe', 'this.LocalSafe();', 'accept'), ('no-call', '', 'accept'))
            for family in ('Rc', 'Arc'):
                for phase in ('constructor', 'destructor'):
                    for case, operation, expected in cases:
                        folder = root / (family + '-' + phase + '-' + case)
                        folder.mkdir()
                        ctor = 'public Payload() { ' + (operation if phase == 'constructor' else '') + ' }'
                        dtor = '~Payload() { ' + (operation if phase == 'destructor' else '') + ' }'
                        source = ('using NativeSharp.Core; class Payload : Base { public static Payload LocalAlias; '
                                  'public void LocalPublish() { LocalAlias = this; } public void LocalSafe() { } ' +
                                  ctor + dtor + ' } class Program { public static int Main() { ' + family +
                                  '<Payload> owner = SharedPointers.Make' + family + '<Payload>(); return 0; } }')
                        (folder / 'Main.ns').write_text(source, encoding='utf-8')
                        consumer = folder / 'Consumer.nsproj'
                        consumer.write_text(project('ReceiverInstanceConsumer', 'Main.ns', [core, library], 'Exe'), encoding='utf-8')
                        label = (family, phase, case)
                        if invoke([compiler, '--check-project-method-bodies', consumer]).returncode != 0:
                            failures.append(label + ('semantic',))
                            continue
                        executable = folder / 'probe.exe'
                        emitted = invoke([compiler, '--emit-project-oop-exe', consumer, '-O0', '-o', executable, '--no-artifact-cache'])
                        text = emitted.stdout + emitted.stderr
                        diagnostic = ('shared payload receiver invokes a callable with unverifiable body'
                                      if expected == 'unverifiable' else
                                      'shared payload publishes its receiver during construction or destruction')
                        if expected == 'accept':
                            if emitted.returncode != 0 or not executable.is_file(): failures.append(label + ('unexpected-rejection',))
                        elif emitted.returncode == 0 or diagnostic not in text:
                            failures.append(label + ('wrong-rejection',))
                folder = root / (family + '-uncertified-base')
                folder.mkdir()
                source = ('using NativeSharp.Core; class Payload : UnsafeBase { public Payload() { } } '
                          'class Program { public static int Main() { ' + family +
                          '<Payload> owner = SharedPointers.Make' + family + '<Payload>(); return 0; } }')
                (folder / 'Main.ns').write_text(source, encoding='utf-8')
                consumer = folder / 'Consumer.nsproj'
                consumer.write_text(project('ReceiverInstanceConsumer', 'Main.ns', [core, library], 'Exe'), encoding='utf-8')
                label = (family, 'uncertified-base')
                if invoke([compiler, '--check-project-method-bodies', consumer]).returncode != 0:
                    failures.append(label + ('semantic',))
                    continue
                emitted = invoke([compiler, '--emit-project-oop-exe', consumer, '-O0', '-o',
                                  folder / 'probe.exe', '--no-artifact-cache'])
                if emitted.returncode == 0 or 'shared payload class has no verifiable precompiled lifecycle body' not in emitted.stdout + emitted.stderr:
                    failures.append(label + ('wrong-rejection',))
    after = hashes()
    print('SHA256 AFTER', after, flush=True)
    if before != after: failures.append(('input-hash-changed',))
    if failures: print('FAIL', failures, file=sys.stderr, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
