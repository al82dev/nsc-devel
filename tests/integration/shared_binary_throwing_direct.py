"""Directed binary-producer regression; all generated files remain under A:."""
from html import escape
from pathlib import Path
import hashlib
import os
import subprocess
import sys
from isolated_a_runner import absolute_a, setup

REPOSITORY = Path(__file__).resolve().parents[2]
FIXTURES = Path(__file__).resolve().parent / 'fixtures' / 'binary_throwing_direct'
CASES = {
    1: (1, 1, 0),
    2: (1, 1, 0),
    3: (1, 2, 0),
    4: (1, 2, 0),
    0: (0, 2, 1),
}

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def reference(library, metadata):
    return ('<LibraryReference Include="' + escape(library.as_posix(), quote=True) +
            '" Metadata="' + escape(metadata.as_posix(), quote=True) + '"/>')

def invoke(command, environment, log, timeout):
    args = [str(item) for item in command]
    try:
        result = subprocess.run(args, cwd=REPOSITORY, env=environment, capture_output=True,
                                text=True, errors='replace', timeout=timeout)
        code = result.returncode
        output = result.stdout + result.stderr
    except subprocess.TimeoutExpired:
        code = None
        output = 'TIMEOUT\n'
    log.write_text('COMMAND ' + subprocess.list2cmdline(args) + '\nCWD ' + str(REPOSITORY) +
                   '\nEXIT ' + str(code) + '\n' + output, encoding='utf-8')
    print(log.name, 'exit=', code, flush=True)
    return code

def main():
    if len(sys.argv) != 5:
        raise SystemExit('expected nsc, Core.lib, Core.nsmeta and new evidence directory')
    compiler, core, metadata, root = (absolute_a(value) for value in sys.argv[1:])
    if any(path.drive.lower() != 'a:' for path in (compiler, core, metadata, root)):
        raise SystemExit('compiler, Core, metadata and evidence must be on A:')
    environment = setup(root, REPOSITORY)
    before = {str(path): digest(path) for path in (compiler, core, metadata)}
    (root / 'inputs-before.txt').write_text(repr(before) + '\n', encoding='utf-8')
    producer_source = (FIXTURES / 'Producer.ns').read_text(encoding='utf-8')
    consumer_source = (FIXTURES / 'Consumer.ns').read_text(encoding='utf-8')
    failures = []
    for producer_level in ('O0', 'O2'):
        producer_dir = root / ('producer-' + producer_level)
        producer_dir.mkdir()
        (producer_dir / 'Producer.ns').write_text(producer_source, encoding='utf-8')
        project = producer_dir / 'Producer.nsproj'
        project.write_text('<Project><PropertyGroup><Name>BinaryThrowingRoot</Name>'
            '<OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup>'
            '<Compile Include="Producer.ns"/>' + reference(core, metadata) +
            '</ItemGroup></Project>', encoding='utf-8')
        library = producer_dir / 'Root.lib'
        if invoke([compiler, '--emit-project-oop-lib', project, '-' + producer_level,
                   '-o', library, '--no-artifact-cache'], environment,
                  producer_dir / 'compile.log', 120) != 0:
            failures.append((producer_level, 'producer'))
            continue
        for mode, (caught, payloads, roots) in CASES.items():
            for consumer_level in ('O0', 'O2'):
                label = 'case-' + str(mode) + '-' + consumer_level
                case_dir = producer_dir / label
                case_dir.mkdir()
                code = consumer_source
                for key, value in (('MODE', mode), ('EXPECTED_CAUGHT', caught),
                                   ('EXPECTED_PAYLOADS', payloads), ('EXPECTED_ROOT', roots)):
                    code = code.replace(key, str(value))
                (case_dir / 'Consumer.ns').write_text(code, encoding='utf-8')
                project = case_dir / 'Consumer.nsproj'
                project.write_text('<Project><PropertyGroup><Name>BinaryThrowingConsumer</Name>'
                    '</PropertyGroup><ItemGroup><Compile Include="Consumer.ns"/>' +
                    reference(core, metadata) + reference(library, library.with_suffix('.nsmeta')) +
                    '</ItemGroup></Project>', encoding='utf-8')
                executable = case_dir / 'case.exe'
                if invoke([compiler, '--emit-project-oop-exe', project, '-' + consumer_level,
                           '-o', executable, '--no-artifact-cache'], environment,
                          case_dir / 'compile.log', 120) != 0:
                    failures.append((producer_level, mode, consumer_level, 'compile'))
                elif invoke([executable], environment, case_dir / 'run.log', 10) != 42:
                    failures.append((producer_level, mode, consumer_level, 'run'))
    after = {str(path): digest(path) for path in (compiler, core, metadata)}
    (root / 'inputs-after.txt').write_text(repr(after) + '\n', encoding='utf-8')
    if before != after:
        failures.append(('input hash changed',))
    (root / 'failures.txt').write_text(repr(failures) + '\n', encoding='utf-8')
    print('FAILURES', failures, flush=True)
    return bool(failures)

if __name__ == '__main__':
    sys.exit(main())
