"""Run the three unchanged SIMD mask reproductions from the limitations report."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    if os.name != 'nt':
        return 77
    compiler = Path(sys.argv[1]).resolve()
    fixtures = Path(__file__).resolve().parent / 'fixtures' / 'simd_mask_report_original'
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-report-mask-') as temporary:
        root = Path(temporary)
        for case in ('mask_index', 'mask_conditional', 'mask_to_vector'):
            directory = root / case
            directory.mkdir()
            shutil.copyfile(fixtures / (case + '.ns'), directory / 'Program.ns')
            project = directory / 'Probe.nsproj'
            project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                               '<ItemGroup><Compile Include="Program.ns" /></ItemGroup></Project>',
                               encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = directory / (level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(exe), '--no-artifact-cache']
                build = subprocess.run(command, cwd=directory, capture_output=True, text=True,
                                       errors='replace', timeout=120)
                if build.returncode:
                    failures.append((case, level, 'compile', build.returncode, build.stdout + build.stderr))
                    continue
                try:
                    run = subprocess.run([str(exe)], cwd=directory, capture_output=True, text=True,
                                         errors='replace', timeout=10)
                except subprocess.TimeoutExpired:
                    failures.append((case, level, 'timeout'))
                    continue
                print(case, level, 'exit=' + str(run.returncode), flush=True)
                if run.returncode != 0:
                    failures.append((case, level, 'run', run.returncode, run.stdout + run.stderr))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
