"""Verify checked i128-to-long conversion at the signed boundaries."""

from pathlib import Path
import subprocess
import sys

from isolated_a_runner import absolute_a, setup


CASES = {
    'overflow': ('''long Convert(i128 value) { return checked((long)value); }
int Main() { i128 value = (i128)9223372036854775807l + (i128)1;
 long result = Convert(value); return result < 0l ? 31 : 32; }''', 147),
    'underflow': ('''long Convert(i128 value) { return checked((long)value); }
int Main() { i128 value = (i128)(-9223372036854775807l) - (i128)2;
 long result = Convert(value); return result > 0l ? 31 : 32; }''', 147),
    'limits': ('''long Convert(i128 value) { return checked((long)value); }
int Main() { i128 max = (i128)9223372036854775807l;
 i128 min = -max - (i128)1;
 if (Convert(max) != 9223372036854775807l) return 1;
 if (Convert(min) != (-9223372036854775807l - 1l)) return 2;
 if (Convert((i128)-1) != -1l || Convert((i128)0) != 0l) return 3;
 return 42; }''', 42),
    'ordinary': ('''long Convert(i128 value) { return (long)value; }
int Main() { i128 value = (i128)9223372036854775807l + (i128)1;
 return Convert(value) < 0l ? 42 : 1; }''', 42),
}


def main():
    if len(sys.argv) != 3:
        raise SystemExit('expected nsc and new evidence directory on A:')
    compiler, root = (absolute_a(value) for value in sys.argv[1:])
    repository = Path(__file__).resolve().parents[2]
    environment = setup(root, repository)
    failures = []
    for name, (source, expected) in CASES.items():
        case = root / name
        case.mkdir()
        (case / 'Program.ns').write_text(source, encoding='utf-8')
        project = case / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Program.ns"/></ItemGroup></Project>', encoding='utf-8')
        for level in ('O0', 'O2'):
            exe = case / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                       '-o', str(exe), '--no-artifact-cache']
            print('CWD', repository, 'COMMAND', subprocess.list2cmdline(command), flush=True)
            result = subprocess.run(command, cwd=repository, env=environment, capture_output=True,
                                    text=True, errors='replace', timeout=120)
            if result.returncode:
                failures.append((name, level, 'compile', result.stdout + result.stderr))
                continue
            result = subprocess.run([str(exe)], cwd=repository, env=environment,
                                    capture_output=True, text=True, errors='replace', timeout=10)
            print('RUN', name, level, result.returncode, 'EXPECTED', expected, flush=True)
            if result.returncode != expected:
                failures.append((name, level, 'run', result.returncode))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
