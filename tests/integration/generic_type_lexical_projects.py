"""Check visibility of generic types from referenced projects."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, root):
    result = subprocess.run([str(value) for value in command], cwd=root, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline([str(value) for value in command]),
          'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-generic-project-visibility-') as temporary:
        root = Path(temporary)
        (root / 'Producer.ns').write_text('''namespace Alpha {
 public struct Box<T> { public int Value() { return 42; } }
 internal struct Hidden<T> { public int Value() { return 42; } }
}
''', encoding='utf-8')
        producer = root / 'Producer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>VisibilityProducer</Name>'
                            '<OutputType>StaticLibrary</OutputType></PropertyGroup>'
                            '<ItemGroup><Compile Include="Producer.ns"/></ItemGroup></Project>', encoding='utf-8')
        consumer = root / 'Consumer.nsproj'
        consumer.write_text('<Project><PropertyGroup><Name>VisibilityConsumer</Name></PropertyGroup>'
                            '<ItemGroup><Compile Include="Main.ns"/><ProjectReference Include="Producer.nsproj"/>'
                            '</ItemGroup></Project>', encoding='utf-8')
        cases = {
            'unimported': ('', 'Box<int>', False),
            'imported': ('using Alpha;', 'Box<int>', True),
            'qualified': ('', 'Alpha.Box<int>', True),
            'alias': ('using Select = Alpha;', 'Select.Box<int>', True),
            'internal': ('using Alpha;', 'Hidden<int>', False),
        }
        for name, (imports, type_name, accepted) in cases.items():
            (root / 'Main.ns').write_text(imports + 'class Program { public static int Main() { '
                                          + type_name + ' value; return value.Value(); } }', encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / (name + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', consumer, '-' + level, '-o', exe,
                              '--no-artifact-cache'], root)
                if accepted:
                    if result.returncode or run([exe], root).returncode != 42:
                        failures.append((name, level, 'expected success'))
                elif (not result.returncode or
                      'does not name a unique visible generic class/struct template' not in
                      result.stdout + result.stderr):
                    failures.append((name, level, 'expected rejection'))
    if failures:
        print('FAIL', failures, file=sys.stderr, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
