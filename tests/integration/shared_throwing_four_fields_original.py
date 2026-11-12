"""Run the four-field shared-factory reproduction with isolated inputs on A:."""

from html import escape
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

from isolated_a_runner import absolute_a, setup


REPOSITORY = Path(__file__).resolve().parents[2]
SOURCE = REPOSITORY / 'tests' / 'integration' / 'fixtures' / 'shared_throwing_four_fields_original.ns'


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def snapshot(files, imports):
    values = {str(path): digest(path) for path in files}
    for path in sorted(imports.rglob('*')):
        if path.is_file():
            values[str(path)] = digest(path)
    return values


def invoke(command, environment, log, timeout):
    args = [str(value) for value in command]
    try:
        result = subprocess.run(args, cwd=REPOSITORY, env=environment,
                                capture_output=True, text=True, errors='replace', timeout=timeout)
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
    if len(sys.argv) != 7:
        raise SystemExit('expected nsc, Core.lib, Core.nsmeta, runtime.lib, import-directory and new evidence directory')
    compiler, core, metadata, runtime, imports, root = (absolute_a(value) for value in sys.argv[1:])
    files = (SOURCE, compiler, core, metadata, runtime)
    if not all(path.is_file() for path in files) or not imports.is_dir():
        raise SystemExit('source, compiler, Core, metadata, runtime and import directory must exist')
    environment = setup(root, REPOSITORY)
    environment['NS_LIB_ROOT'] = str(core.parent)
    environment['NSC_LINKER'] = 'nslink.exe'
    environment['NSC_WINDOWS_RUNTIME_LIBRARY'] = str(runtime)
    environment['NSC_WINDOWS_RUNTIME_IMPORT_LIBRARY_DIRECTORY'] = str(imports)
    before = snapshot(files, imports)
    (root / 'inputs-before.json').write_text(json.dumps(before, indent=2), encoding='utf-8')
    copied = root / 'Main.ns'
    shutil.copyfile(SOURCE, copied)
    if digest(copied) != before[str(SOURCE)]:
        raise SystemExit('copied source differs from the original fixture')
    project = root / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>SharedOriginal</Name></PropertyGroup>'
                       '<ItemGroup><Compile Include="Main.ns"/><LibraryReference Include="' +
                       escape(core.as_posix(), quote=True) + '" Metadata="' +
                       escape(metadata.as_posix(), quote=True) + '"/></ItemGroup></Project>',
                       encoding='utf-8')
    failures = []
    for level in ('O0', 'O2'):
        executable = root / (level + '.exe')
        command = [compiler, '--emit-project-oop-exe', project, '-' + level,
                   '-o', executable, '--no-artifact-cache', '--no-error-log']
        if invoke(command, environment, root / (level + '-compile.log'), 120) != 0:
            failures.append((level, 'compile'))
        elif invoke([executable], environment, root / (level + '-run.log'), 10) != 42:
            failures.append((level, 'run'))
    after = snapshot(files, imports)
    (root / 'inputs-after.json').write_text(json.dumps(after, indent=2), encoding='utf-8')
    if before != after:
        failures.append(('input hash changed',))
    if digest(copied) != before[str(SOURCE)]:
        failures.append(('copied source changed',))
    (root / 'failures.json').write_text(json.dumps(failures, indent=2), encoding='utf-8')
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
