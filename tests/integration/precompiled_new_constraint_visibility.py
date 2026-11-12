"""Check new() against public constructors of NativeSharp binary classes.

The first invocation emits consumers in A. A separate --execute-existing
invocation inspects all imports and runs the successfully emitted consumers.
This does not certify compatibility of historical metadata.
"""
from pathlib import Path
from xml.sax.saxutils import escape
from contextlib import nullcontext
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile


def execute_existing(root):
    root = root.resolve()
    if root.drive.upper() != 'A:' or not root.is_dir():
        print('FAIL: evidence directory must exist on A:', file=sys.stderr)
        return 1
    manifest = json.loads((root / 'executables.json').read_text(encoding='utf-8'))
    if not manifest['complete'] or not manifest['executables']:
        print('FAIL: emission did not complete successfully', file=sys.stderr)
        return 1
    emission_failures = manifest['emission_failures']
    env = os.environ.copy()
    for key in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'):
        env[key] = str(root)
    permitted = {'kernel32.dll', 'kernelbase.dll', 'ucrtbase.dll', 'shell32.dll'}
    prepared = []
    failures = []
    def invoke(command):
        try:
            result = subprocess.run(command, cwd=root, env=env, capture_output=True,
                                    text=True, errors='replace', timeout=120)
            output = result.stdout + result.stderr
            code = result.returncode
        except subprocess.TimeoutExpired:
            code, output = 124, 'TIMEOUT after 120 seconds\n'
        except OSError as error:
            code, output = 127, 'PROCESS LAUNCH FAILED: ' + str(error) + '\n'
        record = ('CWD ' + str(root) + ' COMMAND ' + subprocess.list2cmdline(command) +
                  ' EXIT ' + str(code) + '\n' + output + '\n')
        print(record, flush=True)
        with (root / 'commands.log').open('a', encoding='utf-8') as log:
            log.write(record)
        return code, output
    for entry in manifest['executables']:
        executable = (root / entry['path']).resolve()
        if root not in executable.parents or not executable.is_file():
            failures.append((entry['path'], 'missing-or-outside-evidence'))
            continue
        if hashlib.sha256(executable.read_bytes()).hexdigest() != entry['sha256']:
            failures.append((entry['path'], 'hash-changed'))
            continue
        code, output = invoke(['llvm-readobj', '--coff-imports', str(executable)])
        imports = {name.lower() for name in re.findall(r'^\s*Name:\s*(\S+)', output, re.MULTILINE)}
        if code or not imports or not imports <= permitted:
            failures.append((entry['path'], 'unexpected-or-unverified-imports', sorted(imports)))
            continue
        prepared.append(executable)
    if not failures:
        for executable in prepared:
            code, _ = invoke([str(executable)])
            if code:
                failures.append((str(executable), 'execution', code))
    with (root / 'commands.log').open('a', encoding='utf-8') as log:
        log.write('EMISSION FAILURES ' + repr(emission_failures) + '\n')
        log.write('EXECUTION FAILURES ' + repr(failures) + '\n')
    if emission_failures or failures:
        print('FAIL', {'emission': emission_failures, 'execution': failures}, file=sys.stderr, flush=True)
    return int(bool(emission_failures or failures))


def main():
    if len(sys.argv) == 3 and sys.argv[1] == '--execute-existing':
        return execute_existing(Path(sys.argv[2]))
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
    executables = []
    with nullcontext(tempfile.mkdtemp(prefix='nsc-precompiled-new-', dir=temp_name)) as temporary:
        root = Path(temporary)
        print('EVIDENCE', root, flush=True)
        env = os.environ.copy()
        for key in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'): env[key] = str(root)
        with (root / 'commands.log').open('w', encoding='utf-8') as log:
            log.write('SHA256 BEFORE ' + repr(before) + '\n')
            for key in ('NSC_LINKER', 'NSC_WINDOWS_RUNTIME_LIBRARY',
                        'NSC_WINDOWS_RUNTIME_IMPORT_LIBRARY_DIRECTORY'):
                log.write(key + '=' + env.get(key, '<unset>') + '\n')
        def attr(path): return escape(path.as_posix(), {'"': '&quot;'})
        def reference(path):
            return '<LibraryReference Include="' + attr(path) + '" Metadata="' + attr(path.with_suffix('.nsmeta')) + '"/>'
        def project(name, refs, output):
            return ('<Project><PropertyGroup><Name>' + name + '</Name><OutputType>' + output +
                    '</OutputType></PropertyGroup><ItemGroup><Compile Include="Main.ns"/>' +
                    ''.join(reference(path) for path in refs) + '</ItemGroup></Project>')
        def invoke(command):
            command = [str(x) for x in command]
            try:
                result = subprocess.run(command, cwd=repo, env=env, capture_output=True,
                                        text=True, errors='replace', timeout=120)
            except subprocess.TimeoutExpired as error:
                def output_text(value):
                    return value.decode(errors='replace') if isinstance(value, bytes) else (value or '')
                result = subprocess.CompletedProcess(command, 124, output_text(error.stdout),
                    output_text(error.stderr) + '\nTIMEOUT after 120 seconds\n')
            except OSError as error:
                result = subprocess.CompletedProcess(command, 127, '',
                    'PROCESS LAUNCH FAILED: ' + str(error) + '\n')
            record = ('CWD ' + str(repo) + ' COMMAND ' + subprocess.list2cmdline(command) +
                      ' EXIT ' + str(result.returncode) + '\n' + result.stdout + result.stderr + '\n')
            print(record, flush=True)
            with (root / 'commands.log').open('a', encoding='utf-8') as log:
                log.write(record)
            return result
        producer_dir = root / 'producer'
        producer_dir.mkdir()
        (producer_dir / 'Main.ns').write_text('''class Hidden { }
public class Open { }
public class PublicCtor { public PublicCtor() { } }
public class PrivateCtor { private PrivateCtor() { } }
public class ProtectedCtor { protected ProtectedCtor() { } }
public static class BinaryFactory {
    public static owned T Make<T>() where T : new() { return new T(); }
    public static owned T MakeArg<T>(T value) where T : new() { return new T(); }
    public static owned T Good<T, U>() where T : new() { return new T(); }
    public static owned U Bad<T, U>() where T : new() { return new U(); }
    public static int RunLambda<T>() where T : new() { var make = () => new T(); make(); return 0; }
    public static int RunGenericLambda<T>() where T : new() { var make = <U>() => new U(); make<T>(); return 0; }
    public static int RunShadowLambda<T>() where T : new() { var make = <T>() => new T(); make<T>(); return 0; }
    public static int MakeReal<U>() { return 1; }
    public static int RunReal<T>() { var MakeReal = <U>() => 0; int a = MakeReal<T>(); int b = BinaryFactory.MakeReal<T>(); return a + b - 1; }
}
namespace Foreign { public class NoReference { } }''', encoding='utf-8')
        producer = producer_dir / 'Producer.nsproj'
        producer.write_text(project('VisibilityProducer', [], 'StaticLibrary'), encoding='utf-8')
        library = producer_dir / 'producer.lib'
        built = invoke([compiler, '--emit-project-oop-lib', producer, '-O0', '-o', library])
        if built.returncode != 0 or not library.is_file() or not library.with_suffix('.nsmeta').is_file():
            failures.append(('producer-or-metadata',))
        else:
            cases = [
                ('local-generic', 'class Payload { }', 'owned Payload p = LocalFactory.Make<Payload>();', 'accept'),
                ('local-direct', 'class Payload { }', 'owned Payload p = new Payload();', 'accept'),
                ('local-private', 'class Payload { private Payload() { } }', 'Rc<Payload> p = SharedPointers.MakeRc<Payload>();', 'new-constraint'),
                ('binary-factory-implicit', 'class Payload { }', 'owned Payload p = BinaryFactory.Make<Payload>();', 'accept'),
                ('binary-factory-private', 'class Payload { private Payload() { } }', 'owned Payload p = BinaryFactory.Make<Payload>();', 'new-constraint'),
                ('binary-parameter-good', 'class Payload { }', 'owned Payload p = BinaryFactory.Good<Payload, Payload>();', 'accept'),
                ('binary-parameter-bad', 'class Payload { }', 'owned Payload p = BinaryFactory.Bad<Payload, Payload>();', 'uncertified-parameter'),
                ('binary-lambda-witness', 'class Counter { public static int Seen = 0; } class Base { public Base() { Counter.Seen = 7; } } class Payload : Base { }',
                 'BinaryFactory.RunLambda<Payload>(); if (Counter.Seen != 7) return 1;', 'accept'),
                ('binary-lambda-private', 'class Payload { private Payload() { } }',
                 'BinaryFactory.RunLambda<Payload>();', 'new-constraint'),
                ('binary-generic-lambda-public', 'public class Payload { }',
                 'if (BinaryFactory.RunGenericLambda<Payload>() != 0) return 1;', 'accept'),
                ('binary-generic-lambda-internal', 'class Payload { }',
                 'BinaryFactory.RunGenericLambda<Payload>();', 'uncertified-parameter'),
                ('binary-shadow-lambda-public', 'public class Payload { }',
                 'if (BinaryFactory.RunShadowLambda<Payload>() != 0) return 1;', 'accept'),
                ('binary-shadow-lambda-internal', 'class Payload { }',
                 'BinaryFactory.RunShadowLambda<Payload>();', 'uncertified-parameter'),
                ('binary-lambda-and-real-method', '',
                 'if (BinaryFactory.RunReal<int>() != 0) return 1;', 'accept'),
                ('alias-explicit-homonyms', 'using A = Alpha.Payload; using B = Beta.Payload; namespace Alpha { class Payload { } } namespace Beta { class Payload { } }',
                 'A a = default(A); B b = default(B); owned A x = BinaryFactory.MakeArg<A>(a); owned B y = BinaryFactory.MakeArg<B>(b);', 'accept'),
                ('alias-inferred-homonyms', 'using A = Alpha.Payload; using B = Beta.Payload; namespace Alpha { class Payload { } } namespace Beta { class Payload { } }',
                 'A a = default(A); B b = default(B); owned A x = BinaryFactory.MakeArg(a); owned B y = BinaryFactory.MakeArg(b);', 'accept'),
                ('alias-inferred-homonyms-reverse', 'using A = Alpha.Payload; using B = Beta.Payload; namespace Alpha { class Payload { } } namespace Beta { class Payload { } }',
                 'A a = default(A); B b = default(B); owned B y = BinaryFactory.MakeArg(b); owned A x = BinaryFactory.MakeArg(a);', 'accept'),
                ('alias-inferred-private', 'using A = Alpha.Payload; using B = Beta.Payload; namespace Alpha { class Payload { } } namespace Beta { class Payload { private Payload() { } } }',
                 'A a = default(A); B b = default(B); owned A x = BinaryFactory.MakeArg(a); owned B y = BinaryFactory.MakeArg(b);', 'alias-construction'),
                ('alias-inferred-hidden', 'using A = Outer.Hidden; class Outer { private class Hidden { } }',
                 'A a = default(A); owned A x = BinaryFactory.MakeArg(a);', 'alias-hidden'),
                ('alias-explicit-hidden', 'using A = Outer.Hidden; class Outer { private class Hidden { } }',
                 'A a = default(A); owned A x = BinaryFactory.MakeArg<A>(a);', 'alias-hidden'),
                ('open-box-local', 'class Box<T> { } class User { public static int Use<T>() { var p = SharedPointers.MakeRc<Box<T>>(); return 0; } }',
                 'int value = User.Use<int>();', 'accept'),
                ('open-box-homonym-valid', 'namespace Alpha { public class Box<T> { } } namespace Beta { public class Box<T> { private Box() { } } } class User { public static int Use<T>() { var a = SharedPointers.MakeRc<Alpha.Box<int>>(); return 0; } }',
                 'int value = User.Use<int>();', 'accept'),
                ('open-box-homonym-ab', 'namespace Alpha { public class Box<T> { } } namespace Beta { public class Box<T> { private Box() { } } } class User { public static int Use<T>() { var a = SharedPointers.MakeRc<Alpha.Box<int>>(); var b = SharedPointers.MakeRc<Beta.Box<long>>(); return 0; } }',
                 'int value = User.Use<int>();', 'new-constraint'),
                ('open-box-homonym-ba', 'namespace Alpha { public class Box<T> { } } namespace Beta { public class Box<T> { private Box() { } } } class User { public static int Use<T>() { var b = SharedPointers.MakeRc<Beta.Box<long>>(); var a = SharedPointers.MakeRc<Alpha.Box<int>>(); return 0; } }',
                 'int value = User.Use<int>();', 'new-constraint'),
                ('local-nested-private', 'class Outer { private class Hidden { } }', 'Rc<Outer.Hidden> p = SharedPointers.MakeRc<Outer.Hidden>();', 'nested-private'),
                ('local-nested-private-argument', 'class Outer { private class Hidden { } }',
                 'var p = SharedPointers.MakeRc<Outer.Hidden>();', 'nested-private'),
                ('local-inaccessible-base', 'class Base { private Base() { } } class Payload : Base { }',
                 'Rc<Payload> p = SharedPointers.MakeRc<Payload>();', 'invalid-base'),
                ('foreign-hidden', '', 'Rc<Hidden> p = SharedPointers.MakeRc<Hidden>();', 'hidden'),
                ('foreign-unreferenced', '', 'Rc<Foreign.NoReference> p = SharedPointers.MakeRc<Foreign.NoReference>();', 'unreferenced'),
                ('foreign-private', '', 'Rc<PrivateCtor> p = SharedPointers.MakeRc<PrivateCtor>();', 'new-constraint'),
                ('foreign-protected', '', 'Rc<ProtectedCtor> p = SharedPointers.MakeRc<ProtectedCtor>();', 'new-constraint'),
                ('foreign-direct-open', '', 'owned Open p = new Open();', 'accept'),
            ]
            for family in ('Rc', 'Arc'):
                cases.extend([
                    ('foreign-implicit-' + family, '', family + '<Open> p = SharedPointers.Make' + family + '<Open>();', 'accept'),
                    ('foreign-public-' + family, '', family + '<PublicCtor> p = SharedPointers.Make' + family + '<PublicCtor>();', 'accept'),
                    ('local-public-' + family, 'class Payload { public Payload() { } }', family + '<Payload> p = SharedPointers.Make' + family + '<Payload>();', 'accept'),
                    ('local-implicit-' + family, 'class Payload { }', family + '<Payload> p = SharedPointers.Make' + family + '<Payload>();', 'accept'),
                ])
            for name, declaration, action, expected in cases:
                folder = root / name
                folder.mkdir()
                source = ('using NativeSharp.Core; ' + declaration +
                          ' static class LocalFactory { public static owned T Make<T>() where T : new() { return new T(); } } '
                          'class Program { public static int Main() { ' + action + ' return 0; } }')
                (folder / 'Main.ns').write_text(source, encoding='utf-8')
                consumer = folder / 'Consumer.nsproj'
                refs = [core] if expected == 'unreferenced' else [core, library]
                consumer.write_text(project('FactoryAccessConsumer', refs, 'Exe'), encoding='utf-8')
                checked = invoke([compiler, '--check-project-method-bodies', consumer])
                if expected in ('alias-construction', 'alias-hidden'):
                    rejected = checked
                    if checked.returncode == 0:
                        rejected = invoke([compiler, '--emit-project-oop-exe', consumer, '-O0',
                                           '-o', folder / 'alias-invalid.exe'])
                    output = rejected.stdout + rejected.stderr
                    wanted = ('requires a public parameterless or implicit default constructor',) if expected == 'alias-construction' else (
                        'using alias target', "unknown type name 'A'")
                    if rejected.returncode == 0 or not any(text in output for text in wanted):
                        failures.append((name, 'wrong-alias-rejection'))
                    continue
                if expected == 'uncertified-parameter':
                    rejected = checked
                    if checked.returncode == 0:
                        rejected = invoke([compiler, '--emit-project-oop-exe', consumer, '-O0',
                                           '-o', folder / 'uncertified-parameter.exe'])
                    diagnostic = 'implicit parameterless constructor is not accessible from this module'
                    if rejected.returncode == 0 or diagnostic not in rejected.stdout + rejected.stderr:
                        failures.append((name, 'wrong-parameter-identity-rejection'))
                    continue
                if expected == 'invalid-base':
                    diagnostic = "cannot access the parameterless constructor of its base class"
                    rejected = checked
                    if checked.returncode == 0:
                        rejected = invoke([compiler, '--emit-project-oop-exe', consumer, '-O0',
                                           '-o', folder / 'invalid-base.exe'])
                    if rejected.returncode == 0 or diagnostic not in rejected.stdout + rejected.stderr:
                        failures.append((name, 'wrong-base-constructor-rejection'))
                    continue
                if expected != 'accept':
                    wanted = ('unknown type name' if expected in ('hidden', 'unreferenced') else
                              'is private and is not accessible here' if expected == 'nested-private' else
                              'requires a public parameterless or implicit default constructor')
                    if checked.returncode == 0 or wanted not in checked.stdout + checked.stderr:
                        failures.append((name, 'wrong-semantic-rejection'))
                    continue
                if checked.returncode != 0:
                    failures.append((name, 'semantic'))
                    continue
                levels = ('O0', 'O2') if name.startswith('local-implicit-') or name in (
                    'binary-factory-implicit', 'binary-parameter-good', 'open-box-local',
                    'open-box-homonym-valid', 'binary-lambda-witness', 'binary-generic-lambda-public',
                    'binary-shadow-lambda-public', 'binary-lambda-and-real-method', 'alias-explicit-homonyms',
                    'alias-inferred-homonyms', 'alias-inferred-homonyms-reverse') else ('O0',)
                for level in levels:
                    executable = folder / ('probe-' + level + '.exe')
                    emitted = invoke([compiler, '--emit-project-oop-exe', consumer, '-' + level, '-o', executable])
                    if emitted.returncode != 0 or not executable.is_file():
                        failures.append((name, level, 'emission'))
                        continue
                    executables.append({'path': str(executable.relative_to(root)),
                                        'sha256': hashlib.sha256(executable.read_bytes()).hexdigest()})
        missing_dir = root / 'missing-method-producer'
        missing_dir.mkdir()
        (missing_dir / 'Main.ns').write_text(
            'public class MissingFactory { public static int Run<T>() { return Missing<T>(); } }',
            encoding='utf-8')
        missing_project = missing_dir / 'Producer.nsproj'
        missing_project.write_text(project('MissingProducer', [], 'StaticLibrary'), encoding='utf-8')
        missing = invoke([compiler, '--emit-project-oop-lib', missing_project, '-O0',
                          '-o', missing_dir / 'missing.lib'])
        if missing.returncode == 0 or "missing generic method 'Missing<...>'" not in missing.stdout + missing.stderr:
            failures.append(('missing-method-producer', 'wrong-missing-dependency-rejection'))
    after = hashes()
    print('SHA256 AFTER', after, flush=True)
    if before != after: failures.append(('input-hash-changed',))
    (root / 'executables.json').write_text(json.dumps({
        'complete': True, 'emission_failures': failures, 'executables': executables}, indent=2), encoding='utf-8')
    with (root / 'commands.log').open('a', encoding='utf-8') as log:
        log.write('SHA256 AFTER ' + repr(after) + '\nFAILURES ' + repr(failures) + '\n')
    if failures: print('FAIL', failures, file=sys.stderr, flush=True)
    print('Execution pending: inspect imports and run --execute-existing', root, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
