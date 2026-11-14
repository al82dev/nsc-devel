import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def run(args, cwd):
    return subprocess.run([str(arg) for arg in args], cwd=cwd, capture_output=True,
                          text=True, errors="replace", timeout=120)


def main():
    compiler, core, processor = map(lambda value: Path(value).resolve(), sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="nsc-selfhost-lines-") as temporary:
        root = Path(temporary)
        shutil.copyfile(processor, root / "ComptimeProcessor.ns")
        (root / "Main.ns").write_text(r'''using NativeSharp.Core;
using NativeSharp.Compiler.Source;
class Program { public static int Main() {
 string input = "#run(1+\n2)\nX";
 raii string output = ComptimeProcessor.Transform(input);
 if (output.Length != input.Length || output[0] != '3' || output[7] != '\n' || output[10] != '\n') { return 1; }
 string crlf = "#run(1+\r\n2)\r\nX";
 raii string crlfOutput = ComptimeProcessor.Transform(crlf);
 if (crlfOutput.Length != crlf.Length || crlfOutput[7] != '\r' || crlfOutput[8] != '\n') { return 2; }
 string longValue = "#run(\n123456789012345678\n)\nX";
 raii string longOutput = ComptimeProcessor.Transform(longValue);
 if (longOutput.Length != longValue.Length || longOutput[5] != '\n' || longOutput[6] != '1' ||
     longOutput[23] != '8' || longOutput[24] != '\n' || longOutput[26] != '\n') { return 3; }
 string compact = "#run(\n9*\n9*\n9*\n9*\n9*\n9*\n9*\n9*\n9*\n9*\n9*\n9*\n9*\n9*\n9*\n9*\n9*\n9\n)\nX";
 raii string compactOutput = ComptimeProcessor.Transform(compact);
 if (!String.StartsWith(compactOutput, "150094635296999121") || compactOutput.Length != compact.Length) { return 4; }
 int originalLines = 0; int outputLines = 0; int index = 0;
 while (index < compact.Length) { if (compact[index] == '\n') { originalLines = originalLines + 1; } index = index + 1; }
 index = 0; while (index < compactOutput.Length) { if (compactOutput[index] == '\n') { outputLines = outputLines + 1; } index = index + 1; }
 if (originalLines != outputLines) { return 5; }
 return 0;
} }
''', encoding="utf-8", newline="\n")
        project = root / "Test.nsproj"
        project.write_text('<Project><PropertyGroup><Name>Test</Name></PropertyGroup><ItemGroup>'
                           '<Compile Include="Main.ns" /><Compile Include="ComptimeProcessor.ns" />'
                           '<ProjectReference Include="' + core.as_posix() + '" /></ItemGroup></Project>',
                           encoding="utf-8")
        for level in ("O0", "O2"):
            exe = root / (level + ".exe")
            result = run([compiler, "-" + level, "--emit-project-oop-exe", project, "-o", exe], root)
            assert result.returncode == 0, (level, result.stdout, result.stderr)
            result = run([exe], root)
            assert result.returncode == 0, (level, result.returncode, result.stdout, result.stderr)
            print(level + " PASS")


if __name__ == "__main__":
    main()
