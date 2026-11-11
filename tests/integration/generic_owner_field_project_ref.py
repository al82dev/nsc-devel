from pathlib import Path
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='nsc-field-split-') as tmp:
    root = Path(tmp)
    (root / 'Types.ns').write_text('''namespace First {
public struct Own<const int N> { public int At<pack const int Ns>() { return packat(Ns, N); } } }
namespace Second { public struct Own<const int N> { public int At<pack const int Ns>() { return 7; } } }
''', encoding='utf-8')
    (root / 'Producer.ns').write_text('''using Pick = First;
namespace Producer { public class Wrapper { public Pick.Own<1> value; } }
''', encoding='utf-8')
    (root / 'Consumer.ns').write_text('''using Pick = Second;
class Program { public static int Main() { Producer.Wrapper w = new Producer.Wrapper();
 return w.value.At<20, 42>(); } }
''', encoding='utf-8')
    producer = root / 'Producer.nsproj'
    producer.write_text('<Project><PropertyGroup><Name>FieldProducer</Name></PropertyGroup><ItemGroup>'
                       '<Compile Include="Types.ns"/><Compile Include="Producer.ns"/>'
                       '</ItemGroup></Project>', encoding='utf-8')
    project = root / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>FieldConsumer</Name></PropertyGroup><ItemGroup>'
                       '<Compile Include="Consumer.ns"/><ProjectReference Include="Producer.nsproj"/>'
                       '</ItemGroup></Project>', encoding='utf-8')
    ok = True
    for level in ('O0', 'O2'):
        exe = root / (level + '.exe')
        command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                   '-o', str(exe), '--no-artifact-cache']
        result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
        print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
        if result.returncode:
            ok = False
            continue
        result = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True, timeout=120)
        print('RUN', exe, 'EXIT', result.returncode, flush=True)
        ok = ok and result.returncode == 42
    sys.exit(0 if ok else 1)
