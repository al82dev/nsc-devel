"""Exercise Rc and Arc with a payload from a statically linked producer.

This does not test dynamic module unloading or pinning.
"""
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
    if not core_name or not Path(core_name).is_file() or not Path(core_name).with_suffix('.nsmeta').is_file():
        print('SKIP: NSC_TEST_CORE_BINARY must name an isolated Core library with metadata')
        return 77
    if not temp_name or Path(temp_name).drive.upper() != 'A:' or not Path(temp_name).is_dir():
        print('SKIP: NSC_TEST_TEMP_ROOT or TEMP must name an existing directory on A:')
        return 77
    core = Path(core_name).resolve()
    failures = []
    def hashes():
        return {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in (compiler, core, core.with_suffix('.nsmeta'))}
    before = hashes()
    print('SHA256 BEFORE', before, flush=True)
    with tempfile.TemporaryDirectory(prefix='nsc-static-shared-', dir=temp_name) as temporary:
        root = Path(temporary)
        env = os.environ.copy()
        for key in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'):
            env[key] = str(root)

        def invoke(command):
            command = [str(item) for item in command]
            result = subprocess.run(command, cwd=repo, env=env, capture_output=True,
                                    text=True, errors='replace', timeout=120)
            print('CWD', repo, 'COMMAND', subprocess.list2cmdline(command), 'EXIT',
                  result.returncode, result.stdout + result.stderr, flush=True)
            return result.returncode

        def reference(path):
            return ('<LibraryReference Include="' + escape(path.as_posix(), {'"': '&quot;'}) +
                    '" Metadata="' + escape(path.with_suffix('.nsmeta').as_posix(), {'"': '&quot;'}) + '"/>')

        def project(name, source, libraries, output):
            return ('<Project><PropertyGroup><Name>' + name + '</Name><OutputType>' + output +
                    '</OutputType></PropertyGroup><ItemGroup><Compile Include="' + source + '"/>' +
                    ''.join(reference(path) for path in libraries) + '</ItemGroup></Project>')

        (root / 'Producer.ns').write_text('''public class Payload {
 public static int Drops; public int Data;
 public Payload() { Data = 42; }
 ~Payload() { Drops = Drops + 1; }
}''', encoding='utf-8')
        producer = root / 'Producer.nsproj'
        producer.write_text(project('StaticPayloadProducer', 'Producer.ns', [], 'StaticLibrary'), encoding='utf-8')
        for producer_level in ('O0', 'O2'):
            library = root / ('producer-' + producer_level + '.lib')
            if invoke([compiler, '--emit-project-oop-lib', producer, '-' + producer_level,
                       '-o', library, '--no-artifact-cache']):
                failures.append((producer_level, 'producer'))
                continue
            for family in ('Rc', 'Arc'):
                source = ('''using NativeSharp.Core; class Program { public static int Main() {
 WEAK<Payload> weak;
 { STRONG<Payload> owner = SharedPointers.MakeSTRONG<Payload>();
   weak = owner.Downgrade();
   if (owner.Get().Data != 42 || owner.StrongCount() != 1l || owner.WeakCount() != 1l) return 1;
 }
 if (Payload.Drops != 1 || !weak.Expired()) return 2;
 return 42;
} }''').replace('WEAK', 'Weak' + family).replace('STRONG', family)
                (root / 'Consumer.ns').write_text(source, encoding='utf-8')
                consumer = root / 'Consumer.nsproj'
                consumer.write_text(project('StaticPayloadConsumer', 'Consumer.ns', [core, library], 'Exe'), encoding='utf-8')
                for level in ('O0', 'O2'):
                    label = (producer_level, family, level)
                    executable = root / ('consumer-' + '-'.join(label) + '.exe')
                    if invoke([compiler, '--emit-project-oop-exe', consumer, '-' + level,
                               '-o', executable, '--no-artifact-cache']):
                        failures.append(label + ('build',))
                    elif invoke([executable]) != 42:
                        failures.append(label + ('run',))
    after = hashes()
    print('SHA256 AFTER', after, flush=True)
    if before != after:
        failures.append(('input-hash-changed',))
    if failures:
        print('FAIL', failures, file=sys.stderr, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
