"""Check new() against public constructors of NativeSharp binary classes.

The producer and consumers are compiled in A; consumers are never executed.
This does not certify compatibility of historical metadata.
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
    if not core_name:
        print('SKIP: NSC_TEST_CORE_BINARY is not configured')
        return 77
    if not Path(core_name).is_file() or not Path(core_name).with_suffix('.nsmeta').is_file():
        print('FAIL: configured Core or metadata does not exist', file=sys.stderr)
        return 1
    if not temp_name or not Path(temp_name).is_dir() or Path(temp_name).resolve().drive.upper() != 'A:':
        print('SKIP: NSC_TEST_TEMP_ROOT or TEMP must name a directory on A:')
        return 77
    core = Path(core_name).resolve()
    def hashes():
        return {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in (compiler, core, core.with_suffix('.nsmeta'))}
    before = hashes()
    print('SHA256 BEFORE', before, flush=True)
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-precompiled-new-', dir=temp_name) as temporary:
        root = Path(temporary)
        env = os.environ.copy()
        for key in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'): env[key] = str(root)
        def attr(path): return escape(path.as_posix(), {'"': '&quot;'})
        def reference(path):
            return '<LibraryReference Include="' + attr(path) + '" Metadata="' + attr(path.with_suffix('.nsmeta')) + '"/>'
        def project(name, refs, output):
            return ('<Project><PropertyGroup><Name>' + name + '</Name><OutputType>' + output +
                    '</OutputType></PropertyGroup><ItemGroup><Compile Include="Main.ns"/>' +
                    ''.join(reference(path) for path in refs) + '</ItemGroup></Project>')
        def invoke(command):
            command = [str(x) for x in command]
            result = subprocess.run(command, cwd=repo, env=env, capture_output=True,
                                    text=True, errors='replace', timeout=120)
            print('CWD', repo, 'COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                  result.stdout + result.stderr, flush=True)
            return result
        producer_dir = root / 'producer'
        producer_dir.mkdir()
        (producer_dir / 'Main.ns').write_text('''class Hidden { }
public class Open { }
public class PublicCtor { public PublicCtor() { } }
public class PrivateCtor { private PrivateCtor() { } }
public class ProtectedCtor { protected ProtectedCtor() { } }''', encoding='utf-8')
        producer = producer_dir / 'Producer.nsproj'
        producer.write_text(project('VisibilityProducer', [], 'StaticLibrary'), encoding='utf-8')
        library = producer_dir / 'producer.lib'
        built = invoke([compiler, '--emit-project-oop-lib', producer, '-O0', '-o', library, '--no-artifact-cache'])
        if built.returncode != 0 or not library.is_file() or not library.with_suffix('.nsmeta').is_file():
            failures.append(('producer-or-metadata',))
        else:
            cases = [
                ('local-generic', 'class Payload { }', 'owned Payload p = LocalFactory.Make<Payload>();', 'accept'),
                ('local-direct', 'class Payload { }', 'owned Payload p = new Payload();', 'accept'),
                ('local-private', 'class Payload { private Payload() { } }', 'Rc<Payload> p = SharedPointers.MakeRc<Payload>();', 'new-constraint'),
                ('foreign-hidden', '', 'Rc<Hidden> p = SharedPointers.MakeRc<Hidden>();', 'hidden'),
                ('foreign-private', '', 'Rc<PrivateCtor> p = SharedPointers.MakeRc<PrivateCtor>();', 'new-constraint'),
                ('foreign-protected', '', 'Rc<ProtectedCtor> p = SharedPointers.MakeRc<ProtectedCtor>();', 'new-constraint'),
                ('foreign-direct-open', '', 'owned Open p = new Open();', 'accept'),
            ]
            for family in ('Rc', 'Arc'):
                cases.extend([
                    ('foreign-implicit-' + family, '', family + '<Open> p = SharedPointers.Make' + family + '<Open>();', 'accept'),
                    ('foreign-public-' + family, '', family + '<PublicCtor> p = SharedPointers.Make' + family + '<PublicCtor>();', 'accept'),
                    ('local-public-' + family, 'class Payload { public Payload() { } }', family + '<Payload> p = SharedPointers.Make' + family + '<Payload>();', 'accept'),
                ])
            for name, declaration, action, expected in cases:
                folder = root / name
                folder.mkdir()
                source = ('using NativeSharp.Core; ' + declaration +
                          ' static class LocalFactory { public static owned T Make<T>() where T : new() { return new T(); } } '
                          'class Program { public static int Main() { ' + action + ' return 0; } }')
                (folder / 'Main.ns').write_text(source, encoding='utf-8')
                consumer = folder / 'Consumer.nsproj'
                consumer.write_text(project('FactoryAccessConsumer', [core, library], 'Exe'), encoding='utf-8')
                checked = invoke([compiler, '--check-project-method-bodies', consumer])
                if expected != 'accept':
                    wanted = 'unknown type name' if expected == 'hidden' else 'requires a public parameterless or implicit default constructor'
                    if checked.returncode == 0 or wanted not in checked.stdout + checked.stderr:
                        failures.append((name, 'wrong-semantic-rejection'))
                    continue
                if checked.returncode != 0:
                    failures.append((name, 'semantic'))
                    continue
                executable = folder / 'probe.exe'
                emitted = invoke([compiler, '--emit-project-oop-exe', consumer, '-O0', '-o', executable, '--no-artifact-cache'])
                if emitted.returncode != 0 or not executable.is_file(): failures.append((name, 'emission'))
    after = hashes()
    print('SHA256 AFTER', after, flush=True)
    if before != after: failures.append(('input-hash-changed',))
    if failures: print('FAIL', failures, file=sys.stderr, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
