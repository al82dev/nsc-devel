"""NSC-NCSS-013: layout, isolation and invalid uses of fixed and dynamic decimal arrays."""
import argparse
import os
from pathlib import Path
import subprocess

# sizeof/alignof come from the target decimal layout (16 bytes, alignment 8) that the array,
# stackalloc and raw-element paths already use; the program returns 100 + 10 + 8 when the fixed
# and dynamic arrays keep two distinct values apart and measure that stride and alignment.
VALID = '''int Main()
{
    decimal[2] fixedValues;
    fixedValues[0] = 1.25m;
    fixedValues[1] = -7.5m;
    if (fixedValues[0] != 1.25m || fixedValues[1] != -7.5m) return 1;
    fixedValues[0] = 100.001m;
    if (fixedValues[1] != -7.5m || fixedValues[0] != 100.001m) return 2;
    decimal[] dynamicValues = new decimal[2];
    dynamicValues[0] = 0.1m;
    dynamicValues[1] = -0.2m;
    if (dynamicValues[0] != 0.1m || dynamicValues[1] != -0.2m) return 3;
    dynamicValues[1] = 3.75m;
    if (dynamicValues[0] != 0.1m || dynamicValues[1] != 3.75m) return 4;
    if (dynamicValues.Length != 2) return 5;
    nuint size = sizeof(decimal);
    nuint alignment = alignof(decimal);
    unsafe("measure the element stride and alignment of decimal arrays")
    {
        nuint first = (nuint)&fixedValues[0];
        nuint second = (nuint)&fixedValues[1];
        if (second - first != size) return 6;
        if (first % alignment != 0u) return 7;
        nuint dynamicFirst = (nuint)&dynamicValues[0];
        nuint dynamicSecond = (nuint)&dynamicValues[1];
        if (dynamicSecond - dynamicFirst != size) return 8;
        if (dynamicFirst % alignment != 0u) return 9;
    }
    decimal[2] converted;
    converted[0] = 2;
    converted[1] = (decimal)1.5;
    if (converted[0] != 2.0m || converted[1] != 1.5m) return 10;
    return 100 + (int)size * 10 / 16 + (int)alignment;
}
'''

INVALID = {
    'string element': ('decimal[2] values; values[0] = "x"; return 0;',
                       "cannot implicitly convert assignment RHS from 'string-literal' to 'builtin:decimal'"),
    'decimal index': ('decimal[2] values; values[0.5m] = 1.0m; return 0;',
                      'fixed-array indices must be integral'),
    'narrowing read': ('decimal[2] values; values[0] = 1.0m; int read = values[0]; return read;',
                       "cannot initialize local 'read' of type 'builtin:int' from 'builtin:decimal'"),
    'double write': ('decimal[2] values; values[0] = 1.5; return 0;',
                     "cannot implicitly convert assignment RHS from 'builtin:double' to 'builtin:decimal'"),
    'dynamic string': ('decimal[] values = new decimal[2]; values[1] = "y"; return 0;',
                       "cannot implicitly convert assignment RHS from 'string-literal' to 'builtin:decimal'"),
}

PROJECT = ('<Project><PropertyGroup><Name>DecimalArrayLayout</Name><OutputType>Exe</OutputType>'
           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" /></ItemGroup></Project>')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--linker', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    root = args.work_dir.resolve()
    root.mkdir(parents=True, exist_ok=False)
    env = os.environ.copy()
    env.update(TEMP=str(root), TMP=str(root), NSC_LINKER=str(args.linker),
               NSC_WINDOWS_RUNTIME_LIBRARY=str(args.runtime.resolve()))
    compiler = str(args.compiler.resolve())

    def build(folder, source, level):
        folder.mkdir()
        (folder / 'Program.ns').write_text(source, encoding='utf-8')
        (folder / 'Case.nsproj').write_text(PROJECT, encoding='utf-8')
        return subprocess.run([compiler, '-' + level, '--emit-project-oop-exe', str(folder / 'Case.nsproj'),
                               '-o', str(folder / 'case.exe'), '--no-artifact-cache', '--no-error-log'],
                              cwd=folder, env=env, capture_output=True, text=True, errors='replace',
                              timeout=120)

    failures = []
    for level in ('O0', 'O2'):
        folder = root / ('valid-' + level)
        result = build(folder, VALID, level)
        if result.returncode:
            failures.append((level, 'valid build', result.stdout + result.stderr))
            continue
        run = subprocess.run([str(folder / 'case.exe')], cwd=folder, capture_output=True, timeout=60)
        print(level, 'valid exit', run.returncode, flush=True)
        if run.returncode != 118:
            failures.append((level, 'valid run', run.returncode))
    for index, (name, (body, expected)) in enumerate(INVALID.items()):
        result = build(root / ('invalid-' + str(index)), 'int Main()\n{\n    ' + body + '\n}\n', 'O0')
        output = result.stdout + result.stderr
        print(name, 'exit', result.returncode, flush=True)
        if result.returncode == 0 or expected not in output:
            failures.append((name, result.returncode, output))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('decimal_array_layout: PASS', flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
