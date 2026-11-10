"""Compare lexical visibility of ordinary and generic type names."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(item) for item in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler = Path(sys.argv[1]).resolve()
    cases = {
        'plain-one-no-import': ('', False, False, 'Box'),
        'plain-one-import': ('using Alpha;', False, True, 'Box'),
        'plain-two-import': ('using Alpha;', True, True, 'Box'),
        'plain-two-ambiguous': ('using Alpha; using Beta;', True, False, 'Box'),
        'plain-two-alias': ('using Select = Alpha;', True, True, 'Select.Box'),
        'generic-one-no-import': ('', False, False, 'Box'),
        'generic-one-import': ('using Alpha;', False, True, 'Box'),
        'generic-two-import': ('using Alpha;', True, True, 'Box'),
        'generic-two-ambiguous': ('using Alpha; using Beta;', True, False, 'Box'),
        'generic-two-alias': ('using Select = Alpha;', True, True, 'Select.Box'),
    }
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-generic-type-visibility-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>GenericTypeVisibility</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
        source = root / 'Main.ns'
        for name, (imports, two_namespaces, expected, written_name) in cases.items():
            generic = name.startswith('generic-')
            parameter = '<T>' if generic else ''
            requested = written_name + ('<int>' if generic else '')
            alpha = 'namespace Alpha { public struct Box' + parameter + ' { public int Value() { return 42; } } }'
            beta = ('namespace Beta { public struct Box' + parameter + ' { public int Value() { return 1; } } }'
                    if two_namespaces else '')
            source.write_text(imports + alpha + beta + 'class Program { public static int Main() { '
                              + requested + ' value; return value.Value(); } }', encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                command = [compiler, '--emit-project-oop-exe', project, '-' + level, '-o', executable,
                           '--no-error-log', '--no-artifact-cache']
                result = run(command, root)
                output = result.stdout + result.stderr
                if expected:
                    if result.returncode != 0:
                        failures.append((name, level, 'compile', output))
                        continue
                    execution = run([executable], root)
                    if execution.returncode != 42:
                        failures.append((name, level, 'run', execution.returncode))
                    else:
                        print(name, level, 'exit=42', flush=True)
                elif result.returncode == 0 or not any(
                        text in output for text in ('unknown type name', 'does not name a unique visible',
                                                    'ambiguous')):
                    failures.append((name, level, 'expected visibility rejection', output))
                else:
                    print(name, level, 'rejected', flush=True)
    for failure in failures:
        print('FAIL', failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
