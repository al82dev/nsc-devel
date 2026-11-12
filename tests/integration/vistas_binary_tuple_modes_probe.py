"""Observe tuple-view producer/consumer analysis without executing unsafe programs.

Run from the repository with TEMP and TMP on A:. Pass nsc by name or the
authorized isolated path, the Core binary path and an existing evidence directory.
Each compilation subprocess runs from the repository, not the fixture directory.
This probe records observations; it does not assert that any result is safe.
"""
import contextlib
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


REPO = Path(__file__).resolve().parents[2]
AUTHORIZED_COMPILER = Path('A:/nsc-vistas-evidencia/build/bin/nsc.exe')


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        while True:
            block = stream.read(1024 * 1024)
            if not block:
                break
            value.update(block)
    return value.hexdigest()


def compiler_identity(compiler):
    if compiler.lower() in ('nsc', 'nsc.exe'):
        query = subprocess.run(['cmd', '/d', '/c', 'for %G in (nsc.exe) do @echo %~$PATH:G'],
                               cwd=REPO, capture_output=True, text=True, errors='replace', timeout=30)
        if query.returncode or not query.stdout.strip():
            raise SystemExit('nsc.exe did not resolve through PATH')
        resolved = Path(query.stdout.strip()).resolve()
    else:
        if compiler.replace('\\', '/').lower() != str(AUTHORIZED_COMPILER).replace('\\', '/').lower():
            raise SystemExit('Only the explicitly authorized compiler path may be used')
        resolved = Path(compiler).resolve()
    if resolved != AUTHORIZED_COMPILER.resolve() or not resolved.is_file():
        raise SystemExit('The resolved compiler is not the authorized isolated nsc.exe')
    return resolved


def reference(library):
    return ('<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
            library.with_suffix('.nsmeta').as_posix() + '"/>')


def run(command, records, case):
    try:
        result = subprocess.run(command, cwd=REPO, capture_output=True, text=True,
                                errors='replace', timeout=120)
        record = {'case': case, 'command': command, 'exit': result.returncode,
                  'stdout': result.stdout, 'stderr': result.stderr, 'timed_out': False}
    except subprocess.TimeoutExpired as error:
        record = {'case': case, 'command': command, 'exit': None, 'timed_out': True,
                  'stdout': str(error.stdout or ''), 'stderr': str(error.stderr or '')}
    records.append(record)
    print('CASE', case, 'EXIT', record['exit'], 'TIMEOUT', record['timed_out'], flush=True)
    return record['exit']


def project(name, sources, references, library=False):
    kind = '<OutputType>Library</OutputType>' if library else ''
    return ('<Project><PropertyGroup><Name>' + name + '</Name>' + kind +
            '</PropertyGroup><ItemGroup>' + sources + references + '</ItemGroup></Project>')


def main():
    if len(sys.argv) != 4 or Path.cwd().resolve() != REPO:
        raise SystemExit('Run from this repository: probe <compiler-name> <core.lib> <evidence-dir>')
    compiler, core_arg, evidence_arg = sys.argv[1:]
    core, evidence = Path(core_arg).resolve(), Path(evidence_arg).resolve()
    if core.drive.lower() != 'a:' or evidence.drive.lower() != 'a:':
        raise SystemExit('Core and evidence must be on A:')
    if not core.is_file() or not evidence.is_dir():
        raise SystemExit('Core and evidence directory must already exist')
    for name in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'):
        value = os.environ.get(name, '')
        if not value or Path(value).resolve().drive.lower() != 'a:':
            raise SystemExit(name + ' must point to A: before invoking the probe')
    compiler_file = compiler_identity(compiler)
    identity = {'compiler_path': str(compiler_file), 'compiler_sha256': digest(compiler_file),
                'core_path': str(core), 'core_sha256': digest(core)}
    metadata = core.with_suffix('.nsmeta')
    if metadata.is_file():
        identity['core_metadata_sha256'] = digest(metadata)
    records = []
    cases = {'valid_accepted': [], 'valid_rejected': [],
             'unsafe_accepted': [], 'unsafe_rejected': [], 'timeout': []}
    unexpected = []
    with contextlib.nullcontext(tempfile.mkdtemp(prefix='binary-tuple-modes-', dir=evidence)) as directory:
        root = Path(directory)
        producer, consumer = root / 'producer', root / 'consumer'
        producer.mkdir()
        consumer.mkdir()
        methods = []
        for input_mode in ('in', 'ref'):
            for output_mode in ('ref', 'out'):
                name = 'Save' + input_mode.title() + output_mode.title()
                methods.append(' public static void ' + name + '(' + input_mode +
                    ' ViewTuple pair, ' + output_mode +
                    ' SharedView<Payload> output) { var (view, n) = pair; output = view; }')
        producer_source = ('using NativeSharp.Core; using ViewTuple = (SharedView<Payload>, int); '
            'public class Payload { public int Data; '
            'public Payload() { Data = 42; } } public class Sink {' + ''.join(methods) + '}')
        (producer / 'Main.ns').write_text(producer_source, encoding='utf-8')
        producer_project = producer / 'Producer.nsproj'
        producer_project.write_text(project('VistasTupleModeProducer', '<Compile Include="Main.ns"/>',
                                          reference(core), True), encoding='utf-8')
        library = producer / 'VistasTupleModeProducer.lib'
        status = run([compiler, '--emit-project-oop-lib', str(producer_project), '-O0', '-o',
                      str(library), '--no-artifact-cache'], records, 'producer')
        if status == 0:
            consumer_project = consumer / 'Consumer.nsproj'
            consumer_project.write_text(project('VistasTupleModeConsumer', '<Compile Include="Main.ns"/>',
                                                reference(core) + reference(library)), encoding='utf-8')
            (consumer / 'Producer.ns').write_text(producer_source, encoding='utf-8')
            source_project = consumer / 'SourceConsumer.nsproj'
            source_project.write_text(project('VistasTupleSourceConsumer',
                '<Compile Include="Main.ns"/><Compile Include="Producer.ns"/>', reference(core)),
                encoding='utf-8')
            for family in ('Rc', 'Arc'):
                for input_mode in ('in', 'ref'):
                    for output_mode in ('ref', 'out'):
                        for variant in ('live', 'escape', 'replace'):
                            owner = family + '<Payload> owner = SharedPointers.Make' + family + '<Payload>(); '
                            pair = '(SharedView<Payload>, int) pair = (owner.Borrow(), 0); '
                            result = 'SharedView<Payload> result = default(SharedView<Payload>); '
                            call = ('Sink.Save' + input_mode.title() + output_mode.title() + '('
                                    + input_mode + ' pair, ' + output_mode + ' result); ')
                            replacement = 'owner = SharedPointers.Make' + family + '<Payload>(); '
                            if variant == 'escape':
                                body = result + '{ ' + owner + pair + call + '} return result.Get().Data;'
                            else:
                                body = owner + pair + result + call
                                body += replacement if variant == 'replace' else ''
                                body += 'return result.Get().Data;'
                            source = ('using NativeSharp.Core; class Program { public static int Main() { '
                                      + body + ' } }')
                            source_path = consumer / 'Main.ns'
                            source_path.write_text(source, encoding='utf-8')
                            label = '-'.join((family, input_mode, output_mode, variant))
                            for kind, selected in (('source', source_project), ('binary', consumer_project)):
                                selected_label = label + '-' + kind
                                checked = run([compiler, '--check-project-method-bodies', str(selected)],
                                              records, selected_label)
                                records[-1]['source'] = source
                                if checked is None:
                                    category = 'timeout'
                                elif variant == 'live':
                                    category = 'valid_accepted' if checked == 0 else 'valid_rejected'
                                else:
                                    category = 'unsafe_accepted' if checked == 0 else 'unsafe_rejected'
                                cases[category].append(selected_label)
                                records[-1]['category'] = category
                                output_text = records[-1]['stdout'] + records[-1]['stderr']
                                if kind == 'binary' and (checked == 0 or checked is None or
                                    'contains a borrowed view whose non-escaping contract cannot be verified'
                                    not in output_text):
                                    unexpected.append(selected_label)
                                if kind == 'source' and ((variant == 'live' and checked != 0) or
                                    (variant != 'live' and (checked == 0 or checked is None))):
                                    unexpected.append(selected_label)
        output = root / 'results.json'
        if status != 0:
            records[-1]['note'] = ('The combined producer failed; individual parameter modes are not '
                                   'classified and must be analyzed separately.')
        producer_state = 'timeout' if status is None else 'passed' if status == 0 else 'failed'
        summary = {'producer': producer_state,
                   'planned_consumer_checks': 48,
                   'executed_consumer_checks': sum(len(group) for group in cases.values()),
                   'not_run_consumer_checks': 48 - sum(len(group) for group in cases.values()),
                   'cases': cases, 'unexpected': unexpected,
                   'interpretation': ('A rejection is not proof of lifetime safety; inspect its diagnostic. '
                                      'Accepted unsafe cases are only analyzed, never executed.')}
        output.write_text(json.dumps({'identity': identity, 'producer_source': producer_source,
                                      'summary': summary, 'records': records},
                                     indent=2, ensure_ascii=False), encoding='utf-8')
        print('RESULTS', output, flush=True)
        print('SUMMARY', json.dumps(summary), flush=True)
    return 0 if status == 0 and not unexpected else 1


if __name__ == '__main__':
    sys.exit(main())
