import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1])
    source = '''class Counter { public static int Builds; public static int Drops; }
struct Guard { public Guard(int id) { Counter.Builds = Counter.Builds + 1; }
 ~Guard() { Counter.Drops = Counter.Drops + 1; } }
class Program {
 public static async int Slow() { yield; return 7; }
 public static AsyncIterator<int, int> Gen() {
   (Guard, int) pair = (new Guard(1), await Slow());
   yield return 7;
 }
 public static async int Main() {
   AsyncIterator<int, int> iterator = Gen();
   bool first = await iterator.MoveNextAsync();
   if (!first || Counter.Builds != 1 || Counter.Drops != 0) return 10 + Counter.Builds * 10 + Counter.Drops;
   iterator.Cancel();
   if (Counter.Drops != 0) return 50;
   bool second = await iterator.MoveNextAsync();
   return !second && Counter.Builds == 1 && Counter.Drops == 1 ? 42 : 60 + Counter.Builds * 10 + Counter.Drops;
} }
'''
    with tempfile.TemporaryDirectory(prefix='nsc-tuple-await-cancel-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        (root / 'Main.ns').write_text(source, encoding='utf-8')
        for level in ('O0', 'O2'):
            exe = root / (level + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(project), '-o', str(exe), '-' + level]
            build = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            assert build.returncode == 0, (level, build.stdout, build.stderr)
            if os.name == 'nt':
                result = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                print(f'{level}: exit={result.returncode}', flush=True)
                assert result.returncode == 42, (level, result.returncode, result.stdout, result.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
