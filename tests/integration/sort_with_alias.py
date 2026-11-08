import os
from pathlib import Path
import subprocess
import sys
import tempfile
from xml.sax.saxutils import escape


def run(command, cwd):
    return subprocess.run([str(value) for value in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=180)


def main():
    compiler, collections_project = map(Path, sys.argv[1:3])
    source = '''using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Order { public static int Compare(int left, int right) { return right - left; } }
class Program { public static int Main() {
 owned int[] values = new int[5]; values[0] = 3; values[1] = -1; values[2] = 4; values[3] = 3; values[4] = 0;
 Comparison<int> comparison = Order.Compare;
 Algorithm.SortWith(Spans.Of(values), comparison);
 if (values[0] != 4 || values[1] != 3 || values[2] != 3 || values[3] != 0 || values[4] != -1) return 1;
 Algorithm.SortWith<int>(Spans.Empty<int>(), comparison);
 owned int[] single = new int[1]; single[0] = 17; Algorithm.SortWith(Spans.Of(single), comparison);
 if (single[0] != 17) return 2; return 42;
} }
'''
    with tempfile.TemporaryDirectory(prefix='nsc-sortwith-') as temporary:
        root = Path(temporary)
        (root / 'Main.ns').write_text(source, encoding='utf-8')
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /><ProjectReference Include="' +
                           escape(collections_project.as_posix()) + '" /></ItemGroup></Project>', encoding='utf-8')
        result = run([compiler, '--check-project-method-bodies', project, '--no-banner'], root)
        assert result.returncode == 0, result.stdout + result.stderr
        if os.name == 'nt':
            for level in ('O0', 'O2'):
                exe = root / (level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-o', exe,
                              '--artifact-cache=' + str(root / 'cache'), '-' + level], root)
                assert result.returncode == 0, result.stdout + result.stderr
                result = run([exe], root)
                assert result.returncode == 42, (level, result.returncode, result.stdout, result.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
