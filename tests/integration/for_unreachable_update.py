"""Compile terminating for-loops and check their update is bound but not run."""
import ctypes
import os
from pathlib import Path
import subprocess
import sys

from isolated_a_runner import absolute_a, setup

if len(sys.argv) != 3 or os.name != 'nt':
    raise SystemExit('expected Windows nsc and a new evidence directory on A:')
compiler, root = map(absolute_a, sys.argv[1:])
repository = Path(__file__).resolve().parents[2]
environment = setup(root, repository)
ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
cases = {
    'break': 'int Main() { for (int i = 0; i < 2; i = i + 1) { break; } return 42; }',
    'return': 'int Main() { for (int i = 0; i < 2; i = i + 1) { return 42; } return 1; }',
    'conditional_break': 'int Main() { for (int i = 0; i < 2; i = i + 1) { if (i == 1) break; } return 42; }',
    'continue': 'int Main() { int count = 0; for (int i = 0; i < 2; i = i + 1) '
                '{ count = count + 1; continue; } return count == 2 ? 42 : 1; }',
    'update_not_executed': 'class Program { static int count; static int Step() '
                           '{ count = count + 1; return count; } public static int Main() '
                           '{ for (int i = 0; i < 2; i = Step()) { break; } return count == 0 ? 42 : 1; } }',
    'while_break': 'int Main() { int i = 0; while (i < 2) { break; } return 42; }',
}
failures = []
for name, source in cases.items():
    folder = root / name
    folder.mkdir()
    (folder / 'Main.ns').write_text(source, encoding='utf-8')
    project = folder / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>ForUpdateProbe</Name></PropertyGroup>'
                       '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
    for level in ('O0', 'O2'):
        executable = folder / (level + '.exe')
        command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                   '-o', str(executable), '--no-artifact-cache']
        result = subprocess.run(command, cwd=repository, env=environment, capture_output=True,
                                text=True, errors='replace', timeout=120)
        output = 'COMMAND ' + subprocess.list2cmdline(command) + '\nEXIT ' + str(result.returncode) + '\n'
        output += result.stdout + result.stderr
        if result.returncode == 0:
            run = subprocess.run([str(executable)], cwd=repository, env=environment, capture_output=True,
                                 text=True, errors='replace', timeout=10)
            output += '\nRUN EXIT ' + str(run.returncode) + '\n' + run.stdout + run.stderr
            if run.returncode != 42:
                failures.append((name, level, 'run', run.returncode))
        else:
            failures.append((name, level, 'build', result.returncode))
        (folder / (level + '.log')).write_text(output, encoding='utf-8')
        print(name, level, 'compile=' + str(result.returncode), flush=True)
folder = root / 'unassigned_after_break'
folder.mkdir()
(folder / 'Main.ns').write_text('int Main() { int value; for (int i = 0; i < 2; value = 42) '
                               '{ break; } return value; }', encoding='utf-8')
project = folder / 'Probe.nsproj'
project.write_text('<Project><PropertyGroup><Name>ForUpdateFlow</Name></PropertyGroup>'
                   '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
command = [str(compiler), '--check-project-method-bodies', str(project)]
result = subprocess.run(command, cwd=repository, env=environment, capture_output=True,
                        text=True, errors='replace', timeout=120)
output = result.stdout + result.stderr
(folder / 'check.log').write_text('COMMAND ' + subprocess.list2cmdline(command) + '\nEXIT ' +
                                str(result.returncode) + '\n' + output, encoding='utf-8')
if result.returncode == 0 or "use of unassigned local variable 'value'" not in output:
    failures.append(('unassigned_after_break', 'check', result.returncode))
print('unassigned_after_break', 'check=' + str(result.returncode), flush=True)
print('FAILURES', failures, flush=True)
raise SystemExit(bool(failures))
