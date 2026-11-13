"""Exercise checked i128 narrowing to small signed and unsigned integers."""

from pathlib import Path
import subprocess
import sys

from isolated_a_runner import absolute_a, setup


TYPES = (('short', -32768, 32767), ('sbyte', -128, 127),
         ('byte', 0, 255), ('ushort', 0, 65535), ('char', 0, 255))


def main():
    if len(sys.argv) != 3:
        raise SystemExit('expected nsc and a new evidence directory on A:')
    compiler, root = (absolute_a(value) for value in sys.argv[1:])
    repository = Path(__file__).resolve().parents[2]
    environment = setup(root, repository)
    failures = []
    for typ, minimum, maximum in TYPES:
        for name, value, expected, policy in (
                ('below', minimum - 1, 147, 'checked'),
                ('above', maximum + 1, 147, 'checked'),
                ('minimum', minimum, 42, 'checked'),
                ('maximum', maximum, 42, 'checked'),
                ('zero', 0, 42, 'checked'),
                ('ordinary', maximum + 1, 42, 'ordinary')):
            case = root / (typ + '-' + name)
            case.mkdir()
            conversion = f'({typ})value'
            if policy == 'checked':
                conversion = f'checked({conversion})'
            wrapped = minimum if minimum < 0 else 0
            comparison = f'(i128)result == (i128){wrapped}' if policy == 'ordinary' else '(i128)result == value'
            source = f'''{typ} Convert(i128 value) {{ return {conversion}; }}
int Main() {{ i128 value = (i128){value}; {typ} result = Convert(value);
 return {comparison} ? 42 : 7; }}'''
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
                    failures.append((typ, name, level, 'compile', result.stdout + result.stderr))
                    continue
                result = subprocess.run([str(exe)], cwd=repository, env=environment,
                                        capture_output=True, text=True, errors='replace', timeout=10)
                print('RUN', typ, name, level, result.returncode, 'EXPECTED', expected, flush=True)
                if result.returncode != expected:
                    failures.append((typ, name, level, 'run', result.returncode))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
