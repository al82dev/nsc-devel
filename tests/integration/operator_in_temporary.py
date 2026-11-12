"""Regression for temporary operands of user operators taking `in`."""
from pathlib import Path
import subprocess
import sys
import tempfile


def invoke(command, cwd):
    command = [str(value) for value in command]
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                            errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode, flush=True)
    print(result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    original = Path(sys.argv[2]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-in-operator-temporary-') as temporary:
        root = Path(temporary)
        original_copy = root / 'original'
        original_copy.mkdir()
        for name in ('Program.ns', 'Rational.ns', 'TemporaryInOperator.nsproj'):
            (original_copy / name).write_bytes((original / name).read_bytes())
        (root / 'Rational.ns').write_bytes((original / 'Rational.ns').read_bytes())
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>InOperatorProbe</Name></PropertyGroup>'
            '<ItemGroup><Compile Include="Main.ns"/><Compile Include="Rational.ns"/>'
            '</ItemGroup></Project>', encoding='utf-8')
        cases = {
            'original': (original_copy / 'TemporaryInOperator.nsproj', 0),
            'locals': ('int Main() { Rational a = new Rational(1, 1); '
                       'Rational b = new Rational(2, 2); return a == b ? 42 : 1; }', 42),
            'one-temporary': ('int Main() { Rational a = new Rational(1, 1); '
                              'return a == new Rational(2, 2) ? 42 : 1; }', 42),
            'two-temporaries': ('int Main() { return new Rational(1, 1) == '
                                'new Rational(2, 2) ? 42 : 1; }', 42),
        }
        for name, (source, expected) in cases.items():
            selected = source if isinstance(source, Path) else project
            if not isinstance(source, Path):
                (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                output = root / (name + '-' + level + '.exe')
                result = invoke([compiler, '--emit-project-oop-exe', selected, '-' + level,
                                 '-o', output, '--no-error-log', '--no-artifact-cache'], root)
                if result.returncode:
                    failures.append((name, level, 'compile'))
                    continue
                run = invoke([output], root)
                if run.returncode != expected:
                    failures.append((name, level, 'execute'))
    if failures:
        print('FAIL', failures, file=sys.stderr)
        return 1
    print('operator_in_temporary: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
