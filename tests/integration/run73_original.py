"""Compile the unchanged report example for a #run function call."""

from hashlib import sha256
from pathlib import Path
import subprocess
import sys

from isolated_a_runner import absolute_a, setup


EXPECTED_SOURCE = 'caaf64439020596d775a98541920e9d416f5aba61903e779aa300900de482486'


def main():
    if len(sys.argv) != 3:
        raise SystemExit('expected nsc and a new evidence directory on A:')
    compiler, root = (absolute_a(value) for value in sys.argv[1:])
    repository = Path(__file__).resolve().parents[2]
    environment = setup(root, repository)
    source = Path(__file__).with_name('run73_original_fixture') / 'Program.ns'
    original = source.read_bytes()
    if sha256(original).hexdigest() != EXPECTED_SOURCE:
        raise SystemExit('original fixture hash changed')
    (root / 'Program.ns').write_bytes(original)
    project = root / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                       '<ItemGroup><Compile Include="Program.ns"/></ItemGroup></Project>', encoding='utf-8')
    print('SOURCE_SHA256', EXPECTED_SOURCE, flush=True)
    print('COMPILER_SHA256', sha256(compiler.read_bytes()).hexdigest(), flush=True)
    failures = []
    for level in ('O0', 'O2'):
        executable = root / (level + '.exe')
        command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                   '-o', str(executable), '--no-artifact-cache']
        print('COMMAND', subprocess.list2cmdline(command), 'CWD', repository, flush=True)
        build = subprocess.run(command, cwd=repository, env=environment, capture_output=True,
                               text=True, errors='replace', timeout=120)
        print('BUILD', level, build.returncode, build.stdout + build.stderr, flush=True)
        if build.returncode:
            failures.append((level, 'build'))
            continue
        result = subprocess.run([str(executable)], cwd=repository, env=environment, capture_output=True,
                                text=True, errors='replace', timeout=10)
        print('RUN', level, result.returncode, flush=True)
        if result.returncode != 42:
            failures.append((level, 'run', result.returncode))
    if source.read_bytes() != original or (root / 'Program.ns').read_bytes() != original:
        failures.append(('source changed',))
    print('FAILURES', failures, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
