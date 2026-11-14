"""NSC-NEW-003: a zero decimal literal keeps the scale it is written with.

DecimalLiteralValue promises the digits and scale "verbatim from the text", but the parser returned
scale 0 for every zero, so `0.0m` was stored as `0m`. Programs that measure scale rules from a
zero then measured nothing: `0.0m / -1m` (decimal-multiply-divide, DivZeroResult) and
`max + 0.0m` (decimal-exact-scale-reduction) started from `0m`.

Each check reads the stored scale word (bits 96..127) of a literal or of a result. A zero now keeps
its scale up to 28, a longer zero is clamped to 28 because it has no digits to lose, exponents
still move the scale, the zero stays positive, and numeric equality is unchanged. The invalid case
keeps the 28-digit scale limit for nonzero literals.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

VALID = '''noinline decimal Id(decimal value) { return value; }
noinline decimal Divide(decimal a, decimal b) { return checked(a / b); }
noinline decimal Add(decimal a, decimal b) { return checked(a + b); }

uint Flags(decimal value)
{
    unsafe("read the scale and sign word of a decimal")
    {
        uint* words = (uint*)&value;
        if (words[0] != 0u || words[1] != 0u || words[2] != 0u) return 4294967295u;
        return words[3];
    }
}

int Main()
{
    if (Flags(Id(0m)) != 0u) return 1;
    if (Flags(Id(0.0m)) != 1u) return 2;
    if (Flags(Id(0.00m)) != 2u) return 3;
    if (Flags(Id(0.0000000000000000000000000000m)) != 28u) return 4;
    if (Flags(Id(0.000000000000000000000000000000m)) != 28u) return 5;
    if (Flags(Id(0.0e1m)) != 0u) return 6;
    if (Flags(Id(0e-3m)) != 3u) return 7;
    if (Flags(Id(-0.0m)) != 2147483649u) return 8;
    if (Flags(Divide(0.0m, -1m)) != 2u) return 9;
    if (Flags(Add(0.0m, 0.00m)) != 2u) return 10;
    if (0.0m != 0m || 0.00m != -0.0m) return 11;
    return 0;
}
'''

# The limit for nonzero literals is unchanged. It is reported when the literal is lowered.
INVALID = ('int Main() { decimal tooLong = 0.00000000000000000000000000001m; return 0; }',
           'decimal literal coefficient or scale is out of range')

PROJECT = ('<Project><PropertyGroup><Name>DecimalZeroScale</Name><OutputType>Exe</OutputType>'
           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" /></ItemGroup></Project>')


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=120)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-decimal-zero-') as temporary:
        root = Path(temporary)

        def build(folder, source, level):
            folder.mkdir()
            (folder / 'Program.ns').write_text(source, encoding='utf-8')
            (folder / 'Case.nsproj').write_text(PROJECT, encoding='utf-8')
            return run([compiler, '-' + level, '--emit-project-oop-exe', str(folder / 'Case.nsproj'),
                        '-o', str(folder / 'case.exe'), '--no-artifact-cache', '--no-error-log'], folder)

        for level in ('O0', 'O2'):
            folder = root / ('valid-' + level)
            result = build(folder, VALID, level)
            if result.returncode:
                failures.append((level, 'valid build', result.stdout + result.stderr))
                continue
            executed = run([str(folder / 'case.exe')], folder)
            print(level, 'valid exit', executed.returncode, flush=True)
            if executed.returncode != 0:
                failures.append((level, 'valid run', executed.returncode))
        result = build(root / 'invalid', INVALID[0], 'O0')
        output = result.stdout + result.stderr
        print('nonzero scale 29 exit', result.returncode, flush=True)
        if result.returncode == 0 or INVALID[1] not in output:
            failures.append(('nonzero scale 29', result.returncode, output))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('decimal_zero_literal_scale: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
