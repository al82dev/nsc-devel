"""Exploratory identity observations, not a gate on current yield semantics."""

import difflib
import os
from pathlib import Path
import subprocess
import sys


def main():
    if os.name != 'nt' or len(sys.argv) != 3:
        print('usage: python async_yield_object_identity_probe.py <test-exe> <new-output-dir>')
        return 2
    executable = Path(sys.argv[1]).resolve()
    output = Path(sys.argv[2]).resolve()
    if executable.anchor.lower() != 'a:\\' or output.anchor.lower() != 'a:\\' or not executable.is_file():
        return 2
    output.mkdir(parents=True, exist_ok=False)
    temporary = output / 'temp'
    temporary.mkdir()
    env = os.environ.copy()
    for name in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'):
        env[name] = str(temporary)
    env['PYTHONDONTWRITEBYTECODE'] = '1'
    repository = Path(__file__).resolve().parents[2]
    records = {}
    codes = {}
    polls = {}
    for phase, fixture, workers in (('cold', 'fixture', '1'), ('warm', 'fixture', '1'),
                                    ('parallel', 'fixture-parallel', '4')):
        command = [str(executable), str(output / fixture), 'identity-probe', workers]
        try:
            result = subprocess.run(command, cwd=repository, env=env, capture_output=True,
                                    text=True, errors='replace', timeout=120)
        except subprocess.TimeoutExpired as error:
            (output / (phase + '.log')).write_text(
                'TIMEOUT\nstdout=' + repr(error.stdout) + '\nstderr=' + repr(error.stderr) + '\n',
                encoding='utf-8')
            return 1
        log = ('COMMAND ' + subprocess.list2cmdline(command) + '\nCWD ' + str(repository) +
               '\nEXIT ' + str(result.returncode) + '\n' + result.stdout + result.stderr)
        (output / (phase + '.log')).write_text(log, encoding='utf-8')
        print(log, end='', flush=True)
        codes[phase] = result.returncode
        lines = [line for line in result.stdout.splitlines() if line.startswith('restored-polls=')]
        polls[phase] = int(lines[0].split('=', 1)[1]) if len(lines) == 1 else -1
        records[phase] = [line for line in result.stdout.splitlines()
                          if line.startswith('identity-probe') or line.startswith('identity-mutation')]
    equal = bool(records['cold']) and records['cold'] == records['warm'] == records['parallel']
    summary = ('cold=' + str(codes['cold']) + ' warm=' + str(codes['warm']) +
               ' parallel=' + str(codes['parallel']) + ' records-equal=' + str(equal) +
               ' restored-polls=' + str(polls) + '\n')
    if not equal:
        summary += '\n'.join(difflib.unified_diff(records['cold'], records['warm'],
                                                   fromfile='cold', tofile='warm', lineterm='')) + '\n'
        summary += '\n'.join(difflib.unified_diff(records['cold'], records['parallel'],
                                                   fromfile='cold', tofile='parallel', lineterm='')) + '\n'
    (output / 'comparison.log').write_text(summary, encoding='utf-8')
    print(summary, end='')
    # Mutation results are intentionally observations, not a current product gate.
    return 0 if (all(code == 0 for code in codes.values()) and equal and polls['cold'] == 0 and
                 polls['warm'] > 0 and polls['parallel'] == 0) else 1


if __name__ == '__main__':
    sys.exit(main())
