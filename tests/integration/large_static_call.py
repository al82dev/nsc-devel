import os
from pathlib import Path
import subprocess
import sys


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    root = Path(sys.argv[2]).resolve()
    root.mkdir(parents=True, exist_ok=False)
    for count in (255, 256, 300):
        case = root / str(count)
        case.mkdir()
        parameters = ', '.join('int a' + str(i) for i in range(count))
        arguments = ', '.join('Next()' for _ in range(count))
        source = ('public class Program { private static int calls = 0; '
                  'public static int Next() { calls = calls + 1; return calls; } '
                  'public static int F(' + parameters + ') { '
                  'if (a0 != 1 || a' + str(count - 1) + ' != ' + str(count) +
                  ' || calls != ' + str(count) + ') { return 1; } return 0; } '
                  'public static int Main() { return F(' + arguments + '); } }')
        (case / 'Program.ns').write_text(source, encoding='utf-8')
        (case / 'Case.nsproj').write_text(
            '<Project><PropertyGroup><Name>Case</Name><OutputType>Exe</OutputType>'
            '</PropertyGroup><ItemGroup><Compile Include="Program.ns"/>'
            '</ItemGroup></Project>', encoding='utf-8')
        for opt in ('-O0', '-O2'):
            output = case / (opt[1:] + '.exe')
            result = subprocess.run(
                [compiler, opt, '--artifact-cache=' + str(case / ('cache-' + opt[1:])),
                 '--emit-project-oop-exe', str(case / 'Case.nsproj'), '-o', str(output)],
                cwd=case, env=os.environ.copy(), capture_output=True, text=True, timeout=45)
            if result.returncode:
                raise RuntimeError(f'{count} {opt}: {result.stderr}')
            run = subprocess.run([str(output)], cwd=case, capture_output=True, timeout=10)
            if run.returncode:
                raise RuntimeError(f'{count} {opt}: program returned {run.returncode}')
            print(f'{count} {opt}: PASS', flush=True)


if __name__ == '__main__':
    main()
