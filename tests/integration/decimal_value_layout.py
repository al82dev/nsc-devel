"""NSC-NEW-001: `decimal` has a value layout wherever the compiler asks for one.

ProjectObjectLayoutModel::valueLayoutForTypeKey did not list `builtin:decimal`, so each consumer had
to know it separately. Before the change this program failed to compile in two ways:
"sizeof cannot measure 'builtin:decimal[2]': it has no value layout" and "NativeSharp async future
has no physical layout: future:builtin:decimal". A decimal local kept across an await got no
coroutine frame slot from the same question.

The program measures fixed decimal arrays and a struct with a decimal field (16 bytes, alignment 8:
24 bytes for { decimal, int }), keeps three decimals across a yield and an await, and returns an
async decimal. Each check has its own exit code; 42 is success, at O0 and O2.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

SOURCE = '''struct Price
{
    public decimal Amount;
    public int Units;
}

class P
{
    static int Measure()
    {
        if (sizeof(decimal[2]) != 32u || alignof(decimal[2]) != 8u) return 1;
        if (sizeof(decimal[3]) != 48u) return 2;
        if (sizeof(Price) != 24u || alignof(Price) != 8u) return 3;
        Price price;
        price.Amount = -2.25m;
        price.Units = 3;
        if (price.Amount != -2.25m || price.Units != 3) return 4;
        decimal[2] pair;
        pair[0] = 0.1m;
        pair[1] = 79228162514264337593543950335m;
        if (pair[0] != 0.1m || pair[1] != 79228162514264337593543950335m) return 5;
        return 0;
    }

    public static async decimal Slow() { yield; return 2.5m; }

    public static async int Work()
    {
        decimal kept = 1.25m;
        decimal other = -7.5m;
        decimal large = 79228162514264337593543950335m;
        yield;
        decimal got = await Slow();
        if (kept != 1.25m) return 11;
        if (other != -7.5m) return 12;
        if (large != 79228162514264337593543950335m) return 13;
        if (got != 2.5m) return 14;
        return 0;
    }

    public static async int Main()
    {
        int measured = Measure();
        if (measured != 0) return measured;
        int worked = await Work();
        if (worked != 0) return worked;
        return 42;
    }
}
'''

PROJECT = ('<Project><PropertyGroup><Name>DecimalValueLayout</Name><OutputType>Exe</OutputType>'
           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" /></ItemGroup></Project>')


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=120)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-decimal-layout-') as temporary:
        root = Path(temporary)
        (root / 'Program.ns').write_text(SOURCE, encoding='utf-8')
        (root / 'Case.nsproj').write_text(PROJECT, encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = root / (level + '.exe')
            build = run([compiler, '-' + level, '--emit-project-oop-exe', str(root / 'Case.nsproj'),
                         '-o', str(executable), '--no-artifact-cache', '--no-error-log'], root)
            if build.returncode:
                failures.append((level, 'build', build.stdout + build.stderr))
                continue
            executed = run([str(executable)], root)
            print(level, 'exit', executed.returncode, flush=True)
            if executed.returncode != 42:
                failures.append((level, 'run', executed.returncode))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('decimal_value_layout: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
