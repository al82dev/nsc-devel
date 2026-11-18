"""Pending reproduction: class-result lifetime and binary owned operands for `in` operators."""

import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


SOURCE_TYPES = '''class Payload { public int Value; public Payload() { Value = 42; } }
struct Owner { public owned Payload item; public Owner(int value) { item = new Payload(); }
 public static Payload operator +(in Owner a, in Owner b) { return a.item; } }
'''
SOURCE_CASES = {
    'local-owners': ('''class Program { public static int Main() { Owner a = new Owner(1);
 Owner b = new Owner(2); Payload result = a + b; return result.Value; } }''', True),
    'escape-two-temporaries': ('''class Program { public static Payload Leak() {
 return new Owner(1) + new Owner(2); } public static int Main() { return 42; } }''', False),
    'escape-one-temporary': ('''class Program { public static Payload Leak(Owner local) {
 return local + new Owner(2); } public static int Main() { return 42; } }''', False),
    'borrowed-local-owners': ('''class Program { public static int Main() { Owner a = new Owner(1);
 Owner b = new Owner(2); borrowed Payload result = a + b; return result.Value; } }''', True),
    'borrowed-escape-temporaries': ('''class Program { public static borrowed Payload Leak() {
 return new Owner(1) + new Owner(2); } public static int Main() { return 42; } }''', False),
}
BINARY_TYPES = '''public class State { public static int Log; }
class Part { public int Id; public Part(int id) { Id = id; State.Log = State.Log * 10 + id; }
 ~Part() { State.Log = State.Log * 10 + Id + 3; } }
public struct FieldOwner { private owned Part item;
 public FieldOwner(int id) { item = new Part(id); }
 public static bool operator ==(in FieldOwner a, in FieldOwner b) {
  State.Log = State.Log * 10 + 3; return a.item.Id == 1 && b.item.Id == 2; }
 public static bool operator !=(in FieldOwner a, in FieldOwner b) { return !(a == b); } }
'''
BINARY_CASES = {
    'locals': '''{ FieldOwner a = new FieldOwner(1); FieldOwner b = new FieldOwner(2);
 bool ok = a == b; if (!ok || State.Log != 123) return 1; }
 return State.Log == 12354 ? 42 : 2;''',
    'temporaries': '''bool ok = (new FieldOwner(1) == new FieldOwner(2)) && State.Log == 123;
 if (!ok) return 1; return State.Log == 12354 ? 42 : 2;''',
}


def run(command, repository):
    """Run a compiler operation or an executable from a positive case only."""
    command = [str(part) for part in command]
    result = subprocess.run(command, cwd=repository, capture_output=True, text=True,
                            errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def project(path, name, source, library=None):
    """Create a project whose input and optional binary reference are in A:."""
    reference = ''
    if library is not None and library is not False:
        reference = ('<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                     library.with_suffix('.nsmeta').as_posix() + '"/>')
    path.write_text('<Project><PropertyGroup><Name>' + name + '</Name>' +
                    ('<OutputType>StaticLibrary</OutputType>' if library is False else '') +
                    '</PropertyGroup><ItemGroup><Compile Include="' + source + '"/>' + reference +
                    '</ItemGroup></Project>', encoding='utf-8')


def check_source_cases(compiler, repository, root, failures):
    """Compile and run the positive source case; analyze unsafe cases only."""
    path = root / 'Source.nsproj'
    project(path, 'Source', 'Main.ns')
    for name, (body, valid) in SOURCE_CASES.items():
        types = (SOURCE_TYPES.replace('static Payload operator', 'static borrowed Payload operator')
                 if name.startswith('borrowed-') else SOURCE_TYPES)
        (root / 'Main.ns').write_text(types + body, encoding='utf-8')
        if not valid:
            result = run([compiler, '--check-project-method-bodies', path], repository)
            diagnostics = '\n'.join(line for line in (result.stdout + result.stderr).splitlines()
                                    if re.search(r'\berror(?:\[[^\]]+\])?:', line))
            if result.returncode == 0:
                failures.append((name, 'unsafe result accepted'))
            elif 'no viable' in diagnostics:
                failures.append((name, 'blocked by overload barrier; lifetime not independently verified'))
            elif not any(word in diagnostics.lower() for word in ('borrow', 'lifetime', 'outlives', 'escape')):
                failures.append((name, 'missing lifetime diagnostic'))
            continue
        for level in ('O0', 'O2'):
            executable = root / (name + '-' + level + '.exe')
            built = run([compiler, '--emit-project-oop-exe', path, '-' + level, '-o', executable,
                         '--no-artifact-cache', '--no-error-log'], repository)
            if built.returncode:
                failures.append((name, level, 'compile'))
            elif run([executable], repository).returncode != 42:
                failures.append((name, level, 'execute'))


def check_binary_cases(compiler, repository, root, failures):
    """Produce independent binary operators and test owned operands in each optimization."""
    (root / 'Owner.ns').write_text(BINARY_TYPES, encoding='utf-8')
    producer = root / 'Producer.nsproj'
    project(producer, 'Producer', 'Owner.ns', False)
    for producer_level in ('O0', 'O2'):
        library = root / ('Producer-' + producer_level + '.lib')
        result = run([compiler, '--emit-project-oop-lib', producer, '-' + producer_level, '-o', library,
                      '--no-artifact-cache', '--no-error-log'], repository)
        if result.returncode or not library.is_file() or not library.with_suffix('.nsmeta').is_file():
            failures.append((producer_level, 'producer or metadata'))
            continue
        consumer = root / 'Consumer.nsproj'
        project(consumer, 'Consumer', 'Main.ns', library)
        for name, body in BINARY_CASES.items():
            (root / 'Main.ns').write_text('int Main() { ' + body + ' }', encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + producer_level + '-' + level + '.exe')
                built = run([compiler, '--emit-project-oop-exe', consumer, '-' + level, '-o', executable,
                             '--no-artifact-cache', '--no-error-log'], repository)
                if built.returncode:
                    failures.append((name, producer_level, level, 'compile'))
                elif run([executable], repository).returncode != 42:
                    failures.append((name, producer_level, level, 'execute'))


def check_legacy_barrier(compiler, repository, root, failures):
    """Analyze legacy operators that leak inputs; never execute their consumers."""
    library = repository / 'tests/integration/operator_in_legacy_escape_fixture/Producer.lib'
    digests = {library: 'e0c89507924728b9524647b518c62e11733951b960336ce97e19dd887b8ed836',
               library.with_suffix('.nsmeta'): '7122789f4d72bad33a225549c9ee166f3178a821d59533a3a3ead1a92c7619c0'}
    for path, digest in digests.items():
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            failures.append(('legacy', 'fixture hash', str(path)))
            return
    consumer = root / 'Legacy.nsproj'
    project(consumer, 'Legacy', 'Main.ns', library)
    for kind in ('Direct', 'Alias', 'ViaCall'):
        body = 'int Main() {{ return new {0}(1) + new {0}(2); }}'.format(kind)
        (root / 'Main.ns').write_text(body, encoding='utf-8')
        checked = run([compiler, '--check-project-method-bodies', consumer, '--no-banner'], repository)
        diagnostics = '\n'.join(line for line in (checked.stdout + checked.stderr).splitlines()
                                if re.search(r'\berror(?:\[[^\]]+\])?:', line)).lower()
        if checked.returncode == 0:
            failures.append(('legacy', kind, 'unsafe call accepted'))
        elif not any(word in diagnostics for word in ('no viable', 'borrow', 'lifetime', 'escape')):
            failures.append(('legacy', kind, 'missing safety diagnostic'))


def main():
    compiler = Path(sys.argv[1]).resolve()
    repository = Path(__file__).resolve().parents[2]
    if compiler.drive.casefold() != 'a:':
        raise RuntimeError('compiler must be on A:')
    for variable in ('TEMP', 'TMP'):
        location = os.environ.get(variable)
        if not location or Path(location).resolve().drive.casefold() != 'a:':
            raise RuntimeError(variable + ' must identify a directory on A:')
    if Path(tempfile.gettempdir()).resolve().drive.casefold() != 'a:':
        raise RuntimeError('Python temporary directory must be on A:')
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-vistas-in-provenance-') as temporary:
        root = Path(temporary)
        check_source_cases(compiler, repository, root, failures)
        check_binary_cases(compiler, repository, root, failures)
        check_legacy_barrier(compiler, repository, root, failures)
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
