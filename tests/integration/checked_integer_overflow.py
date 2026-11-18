"""checked(...) on every built-in integer type (LKF-420, NSC-NEW-021).

Only i128 and i128/u128 conversions were checked: checked(int.MaxValue + 1), checked(ulong.MaxValue * 2),
checked(-int.MinValue) and checked((byte)300) silently wrapped. Each overflow must trap with
IntegerOverflow (exit 147) at O0 and O2, each in-range control must return 42, and the default policy
keeps wrapping.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

OVERFLOW = 147

# (name, type, value function body, expression over v(), expected exit code)
CASES = [
    ('int add', 'int', 'return 2147483647;', 'checked(v() + 1)', OVERFLOW),
    ('int sub', 'int', 'return -2147483647 - 1;', 'checked(v() - 1)', OVERFLOW),
    ('int mul', 'int', 'return 2147483647;', 'checked(v() * 2)', OVERFLOW),
    ('int neg', 'int', 'return -2147483647 - 1;', 'checked(-v())', OVERFLOW),
    ('int in range', 'int', 'return 21;', 'checked(v() * 2) == 42 ? 1 : 0', 42),
    ('long mul', 'long', 'return 9223372036854775807;', 'checked(v() * 2L)', OVERFLOW),
    ('long in range', 'long', 'return 21;', 'checked(v() + 21L) == 42L ? 1 : 0', 42),
    ('uint add', 'uint', 'return 0xFFFFFFFF;', 'checked(v() + 1U)', OVERFLOW),
    ('uint sub', 'uint', 'return 0;', 'checked(v() - 1U)', OVERFLOW),
    ('ulong mul', 'ulong', 'return 0xFFFFFFFFFFFFFFFF;', 'checked(v() * 2UL)', OVERFLOW),
    ('ulong wide in range', 'ulong', 'return 0xFFFFFFFFUL;', 'checked(v() * v()) == 0xFFFFFFFE00000001UL ? 1 : 0', 42),
    ('int to byte', 'int', 'return 300;', 'checked((byte)v())', OVERFLOW),
    ('int to byte in range', 'int', 'return 200;', 'checked((byte)v()) == (byte)200 ? 1 : 0', 42),
    ('long to int', 'long', 'return 4294967296;', 'checked((int)v())', OVERFLOW),
    ('int to uint negative', 'int', 'return -1;', 'checked((uint)v())', OVERFLOW),
    ('int to ulong negative', 'int', 'return -1;', 'checked((ulong)v())', OVERFLOW),
    ('uint to int too large', 'uint', 'return 0x80000000;', 'checked((int)v())', OVERFLOW),
    ('int to long in range', 'int', 'return -5;', 'checked((long)v()) == -5L ? 1 : 0', 42),
    ('default policy wraps', 'int', 'return 2147483647;', '(v() + 1) == -2147483647 - 1 ? 1 : 0', 42),
]


def source(type_name, body, expression):
    return ('class Program { static ' + type_name + ' v() { ' + body + ' } public static int Main() { '
            'var r = ' + expression + '; return r == 1 ? 42 : (r == 0 ? 1 : 2); } }')


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=120)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-checked-overflow-') as temporary:
        root = Path(temporary)
        for index, (name, type_name, body, expression, expected) in enumerate(CASES):
            folder = root / ('case-' + str(index))
            folder.mkdir()
            (folder / 'Program.ns').write_text(source(type_name, body, expression), encoding='utf-8')
            project = folder / 'Case.nsproj'
            project.write_text('<Project><PropertyGroup><Name>Case</Name><OutputType>Exe</OutputType>'
                               '</PropertyGroup><ItemGroup><Compile Include="Program.ns"/></ItemGroup></Project>',
                               encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = folder / (level + '.exe')
                build = run([compiler, '-' + level, '--emit-project-oop-exe', str(project), '-o', str(exe),
                             '--no-artifact-cache', '--no-error-log'], folder)
                if build.returncode:
                    failures.append((name, level, 'build', (build.stdout + build.stderr)[-300:]))
                    continue
                executed = run([str(exe)], folder)
                print(name, level, 'exit', executed.returncode, flush=True)
                if executed.returncode != expected:
                    failures.append((name, level, 'exit', executed.returncode, 'expected', expected))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('checked_integer_overflow: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
