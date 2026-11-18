"""Keep a generic type's declaration imports across specialization and cache hits."""
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
    with tempfile.TemporaryDirectory(prefix='nsc-generic-type-import-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>GenericTypeImport</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns"/></ItemGroup></Project>', encoding='utf-8')
        (root / 'Values.ns').write_text('''namespace Alpha { public struct Item<T> {
 public int Value() { return 21; } } }
namespace Beta { public struct Item<T> { public int Value() { return 33; } } }
''', encoding='utf-8')
        main_source = root / 'Main.ns'
        main_text = '''using Pick = Beta;
class Program { public static int Main() { Wrapper<int> value; return value.Read(); } }
'''
        source = root / 'Wrapper.ns'
        cache = root / 'cache'
        executable = root / 'Probe.exe'
        for producer, expected in (('Alpha', 21), ('Beta', 33)):
            source.write_text('using Pick = ' + producer + ''';
public struct Wrapper<T> { private Pick.Item<T> item;
 public int Read() { return item.Value(); } }
''', encoding='utf-8')
            for level in ('O0', 'O2'):
                for phase in ('cold', 'warm', 'changed-consumer'):
                    main_source.write_text(main_text + ('// changed consumer\n' if phase == 'changed-consumer' else ''),
                                           encoding='utf-8')
                    result = run([compiler, '--emit-project-oop-exe', project, '-o', executable,
                                  '-' + level, '--artifact-cache=' + str(cache), '--time'], root)
                    if result.returncode:
                        return 1
                    if phase == 'warm' and 'compilation-bundle-cache: hit' not in result.stdout + result.stderr:
                        print('Missing warm cache hit', flush=True)
                        return 1
                    if phase == 'changed-consumer' and 'compilation-bundle-cache: hit' in result.stdout + result.stderr:
                        print('Changed consumer unexpectedly reused the project bundle', flush=True)
                        return 1
                    result = run([executable], root)
                    if result.returncode != expected:
                        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
