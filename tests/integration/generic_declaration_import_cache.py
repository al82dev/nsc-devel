import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(part) for part in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler = Path(sys.argv[1])
    with tempfile.TemporaryDirectory(prefix='nsc-generic-scope-cache-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="*.ns" />'
                           '</ItemGroup></Project>', encoding='utf-8')
        (root / 'Producers.ns').write_text('''namespace ProducerA {
 public static class Factory { public static int Value<T>() where T : new() { return 21; } }
}
namespace ProducerB {
 public static class Factory { public static int Value<T>() where T : new() { return 33; } }
}
class Payload { }
''', encoding='utf-8')
        (root / 'Consumer.ns').write_text('class Program { public static int Main() '
                                         '{ return Outer.Make<Payload>(); } }', encoding='utf-8')
        outer = root / 'Outer.ns'
        executable = root / 'Probe.exe'
        cache = root / 'cache'
        for namespace, expected in (('ProducerA', 21), ('ProducerB', 33)):
            outer.write_text('using Pick = ' + namespace + ';\nclass Outer { public static int Make<T>() '
                             'where T : new() { return Pick.Factory.Value<T>(); } }', encoding='utf-8')
            for phase in ('cold', 'warm'):
                command = [compiler, '--emit-project-oop-exe', project, '-o', executable,
                           '-O0', '--artifact-cache=' + str(cache), '--time']
                result = run(command, root)
                output = result.stdout + result.stderr
                assert result.returncode == 0, (namespace, phase, output)
                if phase == 'warm':
                    assert 'compilation-bundle-cache: hit' in output, (namespace, phase, output)
                    print(namespace, 'compilation-bundle-cache: hit', flush=True)
                if os.name == 'nt':
                    result = run([executable], root)
                    assert result.returncode == expected, (namespace, phase, result.returncode)
                    print(namespace, phase, 'exit=' + str(expected), flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
