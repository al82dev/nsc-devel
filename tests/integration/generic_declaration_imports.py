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
    namespaces = '''namespace ProducerA {
 public static class Factory { public static int Value<T>() where T : new() { return 21; } }
}
namespace ProducerB {
 public static class Factory { public static int Value<T>() where T : new() { return 33; } }
}
class Payload { }
'''
    variants = {
        'import': ('using ProducerA;', 'Factory.Value<T>()'),
        'alias': ('using Pick = ProducerA;', 'Pick.Factory.Value<T>()'),
        'qualified': ('', 'ProducerA.Factory.Value<T>()'),
        'missing': ('', 'Factory.Value<T>()'),
    }
    with tempfile.TemporaryDirectory(prefix='nsc-generic-declaration-imports-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="*.ns" />'
                           '</ItemGroup></Project>', encoding='utf-8')
        (root / 'Producers.ns').write_text(namespaces, encoding='utf-8')
        (root / 'Consumer.ns').write_text('using ProducerB; class Program { public static int Main() '
                                         '{ return Outer.Make<Payload>(); } }', encoding='utf-8')
        for name, (directive, call) in variants.items():
            (root / 'Outer.ns').write_text(directive + '\nclass Outer { public static int Make<T>() '
                'where T : new() { return ' + call + '; } }', encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', project], root)
            output = result.stdout + result.stderr
            if name == 'missing':
                assert result.returncode != 0 and 'no visible' in output, (name, output)
                continue
            assert result.returncode == 0, (name, output)
            repeated = run([compiler, '--check-project-method-bodies', project], root)
            assert repeated.returncode == 0, (name, repeated.stdout + repeated.stderr)
            if os.name == 'nt':
                for level in ('O0', 'O2'):
                    executable = root / (name + level + '.exe')
                    result = run([compiler, '--emit-project-oop-exe', project, '-o', executable,
                                  '--no-artifact-cache', '-' + level], root)
                    assert result.returncode == 0, (name, level, result.stdout + result.stderr)
                    result = run([executable], root)
                    assert result.returncode == 21, (name, level, result.returncode)
                    print(name, level, 'resolved declaration import', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
