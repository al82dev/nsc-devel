"""LKF-079: `sizeof` and `alignof` accept a closed generic type such as `Box<int>`.

The operand check (Parser::looksLikeSizeofTypeForm) stopped at the type name, so `sizeof(Box<int>)`
was parsed as a comparison and failed with "expected expression". The valid program measures three
closings of one generic struct, a nested closing, and keeps the expression form `sizeof(local)`,
at O0 and O2 (42). The invalid program names an unknown type argument and must report it.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

VALID = '''struct Box<T>
{
    public T Value;
    public int Tag;
}
int Main()
{
    if (sizeof(Box<int>) != 8u || alignof(Box<int>) != 4u) return 1;
    if (sizeof(Box<long>) != 16u || alignof(Box<long>) != 8u) return 2;
    if (sizeof(Box<byte>) != 8u) return 3;
    if (sizeof(Box<Box<long>>) != 24u || alignof(Box<Box<long>>) != 8u) return 4;
    int local = 3;
    if (sizeof(local) != 4u) return 5;
    Box<long> b;
    b.Value = 5;
    b.Tag = 2;
    return (int)b.Value * 8 + b.Tag;
}
'''

INVALID = ('struct Box<T> { public T Value; }\nint Main() { nuint s = sizeof(Box<Missing>); return (int)s; }\n',
           ":2:35: error: unknown type name 'Missing'")

PROJECT = ('<Project><PropertyGroup><Name>SizeofGeneric</Name><OutputType>Exe</OutputType>'
           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" /></ItemGroup></Project>')


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=120)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-sizeof-generic-') as temporary:
        root = Path(temporary)

        def write(folder, source):
            folder.mkdir()
            (folder / 'Program.ns').write_text(source, encoding='utf-8')
            (folder / 'Case.nsproj').write_text(PROJECT, encoding='utf-8')
            return folder / 'Case.nsproj'

        for level in ('O0', 'O2'):
            folder = root / ('valid-' + level)
            case = write(folder, VALID)
            build = run([compiler, '-' + level, '--emit-project-oop-exe', str(case), '-o',
                         str(folder / 'case.exe'), '--no-artifact-cache', '--no-error-log'], folder)
            if build.returncode:
                failures.append((level, 'build', build.stdout + build.stderr))
                continue
            executed = run([str(folder / 'case.exe')], folder)
            print(level, 'exit', executed.returncode, flush=True)
            if executed.returncode != 42:
                failures.append((level, 'run', executed.returncode))
        case = write(root / 'invalid', INVALID[0])
        result = run([compiler, '--check-project-method-bodies', str(case)], case.parent)
        output = result.stdout + result.stderr
        print('unknown type argument exit', result.returncode, flush=True)
        if result.returncode == 0 or INVALID[1] not in output:
            failures.append(('unknown type argument', result.returncode, output))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('sizeof_closed_generic: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
