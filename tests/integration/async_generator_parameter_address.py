import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1])
    source = '''struct Box { public int Value; public Box(int value) { Value = value; }
 public int Get() { return Value; } }
class Program {
 public static async int Slow() { yield; return 1; }
 public static AsyncIterator<int, int> Gen(Box box) {
   yield return box.Get(); int ignored = await Slow(); yield return box.Get();
 }
 public static async int Read(Box box) { int ignored = await Slow(); return box.Get(); }
 public static async int Main() {
   AsyncIterator<int, int> iterator = Gen(new Box(20));
   bool first = await iterator.MoveNextAsync(); int a = iterator.Current;
   bool second = await iterator.MoveNextAsync(); int b = iterator.Current;
   bool third = await iterator.MoveNextAsync(); int c = await Read(new Box(2));
   return first && second && !third && a + b + c == 42 ? 42 : 1;
} }
'''
    with tempfile.TemporaryDirectory(prefix='nsc-agen-parameter-address-') as temporary:
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
