"""Check namespace aliases without weakening project visibility."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1]).resolve()
    repo = Path(__file__).resolve().parents[2]
    fixtures = Path(__file__).with_suffix('')
    expected = {
        'visible': None,
        'hidden': 'Pick.Value',
        'internal-direct': 'External.Hidden',
        'internal-alias': 'Pick.Hidden',
    }
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-alias-visibility-') as temporary:
        root = Path(temporary)
        shutil.copytree(fixtures, root / 'fixtures')
        env = os.environ.copy()
        for key in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'):
            env[key] = str(root)
        for case, missing in expected.items():
            project = root / 'fixtures' / case / 'Probe.nsproj'
            command = [str(compiler), '--check-project-method-bodies', str(project)]
            result = subprocess.run(command, cwd=repo, env=env, capture_output=True,
                                    text=True, errors='replace', timeout=120)
            output = result.stdout + result.stderr
            print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                  output, flush=True)
            if missing is None:
                if result.returncode != 0:
                    failures.append(case)
            elif (result.returncode == 0 or missing not in output or
                  'does not name a unique visible generic class/struct template' not in output):
                failures.append(case)
    if failures:
        print('FAIL', failures, file=sys.stderr, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
