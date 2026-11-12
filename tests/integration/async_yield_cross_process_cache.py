import difflib
import os
from pathlib import Path
import subprocess
import sys


def run_phase(executable, repository, output, environment, phase):
    command = [str(executable), str(output / 'fixture'), phase]
    result = subprocess.run(command, cwd=repository, env=environment, capture_output=True,
                            text=True, errors='replace', timeout=120)
    log = ('COMMAND ' + subprocess.list2cmdline(command) + '\nCWD ' + str(repository)
           + '\nEXIT ' + str(result.returncode) + '\n' + result.stdout + result.stderr)
    (output / (phase + '.log')).write_text(log, encoding='utf-8')
    print(log, end='', flush=True)
    records = [line for line in result.stdout.splitlines()
               if line.startswith('producer ') or line.startswith('wrapper ')]
    return result.returncode, records


def main():
    if os.name != 'nt':
        return 77
    if len(sys.argv) != 3:
        print('usage: python async_yield_cross_process_cache.py <test-exe> <new-output-dir>', file=sys.stderr)
        return 2
    executable = Path(sys.argv[1]).resolve()
    output = Path(sys.argv[2]).resolve()
    if output.anchor.lower() != 'a:\\' or executable.anchor.lower() != 'a:\\' or not executable.is_file():
        print('output and existing test executable must be on A:', file=sys.stderr)
        return 2
    repository = Path(__file__).resolve().parents[2]
    try:
        output.mkdir(parents=True, exist_ok=False)
    except FileExistsError:
        print('output directory already exists; use a new directory', file=sys.stderr)
        return 2
    temporary = output / 'temp'
    temporary.mkdir()
    environment = os.environ.copy()
    for name in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'):
        environment[name] = str(temporary)
    environment['PYTHONDONTWRITEBYTECODE'] = '1'
    cold_code, cold = run_phase(executable, repository, output, environment, 'cold')
    warm_code, warm = run_phase(executable, repository, output, environment, 'warm')
    diff = list(difflib.unified_diff(cold, warm, fromfile='cold', tofile='warm', lineterm=''))
    summary = ('cold_exit=' + str(cold_code) + ' warm_exit=' + str(warm_code)
               + ' cold_records=' + str(len(cold)) + ' warm_records=' + str(len(warm))
               + ' records_equal=' + str(bool(cold) and cold == warm) + '\n')
    if diff:
        summary += '\n'.join(diff) + '\n'
    (output / 'comparison.log').write_text(summary, encoding='utf-8')
    print(summary, end='')
    return 0 if cold_code == 0 and warm_code == 0 and cold and cold == warm else 1


if __name__ == '__main__':
    sys.exit(main())
