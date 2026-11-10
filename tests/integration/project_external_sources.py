"""Exercise explicitly selected external project sources with the real compiler."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(arguments, expected=0, contains=None):
    result = subprocess.run([str(value) for value in arguments], capture_output=True,
                            text=True, errors='replace')
    output = result.stdout + result.stderr
    print('COMMAND', *arguments, 'EXIT', result.returncode, flush=True)
    if output:
        print(output, flush=True)
    assert (result.returncode == 0) == (expected == 0), output
    if contains:
        assert contains in output, output
    return output


def main():
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-102-outside-') as temporary:
        root = Path(temporary)
        project = root / 'Project'
        shared = root / 'Shared'
        nested = shared / 'Nested'
        project.mkdir()
        nested.mkdir(parents=True)
        (shared / 'Main.ns').write_text(
            'class Program { public static int Main() { return 42; } }', encoding='utf-8')
        (nested / 'Ignored.ns').write_text('not valid NativeSharp', encoding='utf-8')
        (nested / 'Additional.ns').write_text('// nested source', encoding='utf-8')
        (shared / 'bin').mkdir()
        (shared / 'bin' / 'Generated.ns').write_text('// reserved', encoding='utf-8')
        (project / 'Local.ns').write_text('// local source', encoding='utf-8')
        descriptor = project / 'Probe.nsproj'

        def write(items):
            descriptor.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                                  '<ItemGroup>' + items + '</ItemGroup></Project>', encoding='utf-8')

        write('')
        dump = run([compiler, '--dump-project', descriptor], contains='Local.ns')
        assert '../Shared/' not in dump
        write('<Compile Include="../Shared/Main.ns"/>')
        dump = run([compiler, '--dump-project', descriptor], contains='../Shared/Main.ns')
        assert 'Local.ns' not in dump
        for level in ('O0', 'O2'):
            executable = project / (level + '.exe')
            run([compiler, '--emit-project-oop-exe', descriptor, '-o', executable,
                 '--no-artifact-cache', '-' + level])
            process = subprocess.run([str(executable)])
            print('EXECUTE', level, process.returncode, flush=True)
            assert process.returncode == 42

        write('<Compile Include="../Shared/**/*.ns" Exclude="../Shared/Nested/**"/>')
        dump = run([compiler, '--dump-project', descriptor], contains='../Shared/Main.ns')
        assert 'Ignored.ns' not in dump
        run([compiler, '--check-project-graph', descriptor])
        for level in ('O0', 'O2'):
            executable = project / ('glob-' + level + '.exe')
            run([compiler, '--emit-project-oop-exe', descriptor, '-o', executable,
                 '--no-artifact-cache', '-' + level])
            process = subprocess.run([str(executable)])
            print('GLOB EXECUTE', level, process.returncode, flush=True)
            assert process.returncode == 42
        write('<Compile Include="..\\Shared\\Nested\\Additional.ns"/>')
        run([compiler, '--dump-project', descriptor], contains='../Shared/Nested/Additional.ns')
        write('<Compile Include="../Shared/*/Additional.ns"/>')
        run([compiler, '--dump-project', descriptor], contains='../Shared/Nested/Additional.ns')
        write('<Compile Include="../Shared/**/*.ns"/>')
        dump = run([compiler, '--dump-project', descriptor], contains='../Shared/Nested/Additional.ns')
        assert '../Shared/Main.ns' in dump and '../Shared/Nested/Ignored.ns' in dump
        assert 'Generated.ns' not in dump
        write('<Compile Include="Local.ns"/><Compile Include="../Shared/bin/*.ns"/>')
        dump = run([compiler, '--dump-project', descriptor], contains='Local.ns')
        assert 'Generated.ns' not in dump
        write('<Compile Include="../Shared/**/*.ns"/><Compile Remove="../Shared/Nested/**"/>')
        dump = run([compiler, '--dump-project', descriptor], contains='../Shared/Main.ns')
        assert 'Ignored.ns' not in dump
        write('<Compile Include="../Shared/Main.ns;../Shared/./Main.ns;../Shared/*.ns"/>')
        dump = run([compiler, '--dump-project', descriptor], contains='../Shared/Main.ns')
        assert dump.count('../Shared/Main.ns') == 1, dump
        write('<Compile Include="../Shared/*/../Main.ns"/>')
        run([compiler, '--check-project', descriptor], expected=1,
            contains="cannot contain '..' after a wildcard segment")
        write('<Compile Include="C:/outside/*.ns"/>')
        run([compiler, '--check-project', descriptor], expected=1,
            contains='glob pattern must be project-relative')
        write('<Compile Include="../Shared/**Main.ns"/>')
        run([compiler, '--check-project', descriptor], expected=1,
            contains="'**' must occupy an entire glob path segment")
        write('<Compile Include="' + '../' * 40 + '*.ns"/>')
        run([compiler, '--check-project', descriptor], expected=1,
            contains='traverses above the filesystem root')
        write('<Compile Include="../Missing/*.ns"/>')
        run([compiler, '--check-project', descriptor], expected=1,
            contains='must contain at least one .ns source')
        write('<Compile Include="Local.ns"/><Compile Include="../Missing/*.ns"/>')
        run([compiler, '--check-project', descriptor])

        junction_target = root / 'JunctionTarget'
        junction_target.mkdir()
        (junction_target / 'JunctionOnly.ns').write_text('// junction target', encoding='utf-8')
        junction = shared / 'Junction'
        junction_result = subprocess.run(['cmd.exe', '/c', 'mklink', '/J', str(junction),
                                          str(junction_target)], capture_output=True, text=True)
        if junction_result.returncode:
            print('JUNCTION TEST UNAVAILABLE', junction_result.stdout, junction_result.stderr, flush=True)
        else:
            try:
                write('<Compile Include="../Shared/**/*.ns"/>')
                dump = run([compiler, '--dump-project', descriptor], contains='../Shared/Main.ns')
                assert 'JunctionOnly.ns' not in dump, dump
                print('JUNCTION traversal: PASS', flush=True)
            finally:
                junction.rmdir()

        target = root / 'Outside.ns'
        target.write_text('// outside search root', encoding='utf-8')
        link = shared / 'Linked.ns'
        try:
            os.symlink(target, link)
        except OSError as error:
            print('SYMLINK TEST UNAVAILABLE', error, flush=True)
        else:
            write('<Compile Include="../Shared/Linked.ns"/>')
            run([compiler, '--check-project', descriptor], expected=1,
                contains='external project source escapes its search root')
        print('project_external_sources: PASS', flush=True)


if __name__ == '__main__':
    main()
