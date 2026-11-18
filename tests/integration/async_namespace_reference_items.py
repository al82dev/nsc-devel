import os
import ctypes
from pathlib import Path
import subprocess
import sys

compiler = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
repository = Path(__file__).resolve().parents[2]
if os.name != 'nt' or root.anchor.lower() != 'a:\\' or compiler.anchor.lower() != 'a:\\':
    raise SystemExit(2)
ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
root.mkdir(parents=True, exist_ok=False)
failures = []
for mode in ('ref', 'ref mut'):
    for owner in ('class', 'namespace'):
        name = owner + '-' + mode.replace(' ', '-')
        directory = root / name
        directory.mkdir()
        declaration = ('class Helpers { public static ' if owner == 'class' else 'namespace Helpers { public ')
        between = 'value = 13;' if mode == 'ref' else ''
        first = 'first = item;' if mode == 'ref' else 'item = 42; first = item;'
        expected = 'first == 10 && item == 13' if mode == 'ref' else 'first == 42 && item == 42'
        source = (declaration + 'AsyncIterator<' + mode + ' int, int> Gen() '
                  + '{ int value = 10; yield return value; ' + between + ' yield return value; } }\n'
                  + 'class Program { public static int Main() { AsyncIterator<' + mode
                  + ' int, int> it = Helpers.Gen(); bool next = await it.MoveNextAsync(); '
                  + 'if (!next) return 1; int first = 0; { ref int item = ref it.Current; ' + first + ' } '
                  + 'next = await it.MoveNextAsync(); if (!next) return 2; '
                  + '{ ref int item = ref it.Current; return ' + expected + ' ? 42 : 3; } } }\n')
        (directory / 'Main.ns').write_text(source, encoding='utf-8')
        project = directory / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = directory / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(executable), '--no-artifact-cache']
            result = subprocess.run(command, cwd=repository, capture_output=True, text=True,
                                    errors='replace', timeout=120)
            content = ('COMMAND ' + subprocess.list2cmdline(command) + '\nCWD ' + str(repository)
                       + '\nCOMPILE_EXIT ' + str(result.returncode) + '\n' + result.stdout + result.stderr)
            if result.returncode == 0:
                try:
                    run = subprocess.run([str(executable)], cwd=repository, capture_output=True,
                                         text=True, errors='replace', timeout=10)
                    content += 'RUN_EXIT ' + str(run.returncode) + '\n' + run.stdout + run.stderr
                    if run.returncode != 42:
                        failures.append((name, level, 'run', run.returncode))
                except subprocess.TimeoutExpired:
                    content += 'RUN_TIMEOUT\n'
                    failures.append((name, level, 'timeout'))
            else:
                failures.append((name, level, 'compile', result.returncode))
            (directory / (level + '.log')).write_text(content, encoding='utf-8')
            print(name, level, 'compile=' + str(result.returncode), flush=True)
print('FAILURES', failures, flush=True)
raise SystemExit(int(bool(failures)))
