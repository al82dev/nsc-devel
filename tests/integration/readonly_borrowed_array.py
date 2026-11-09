import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(item) for item in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler, *sources = map(Path, sys.argv[1:])
    prefix = '''using NativeSharp.Core.Internal;
namespace NativeSharp.Core { public class Exception { } }
class Payload { public int[] Data; public Payload() { Data = new int[1]; } }
class Child { public int Value; }
class Children { public Child[] Data; public Children() { Data = new Child[1]; Data[0] = new Child(); } }
'''
    cases = {
        'write': ('''alias.Data[0] = 10; return 0;''', 'not assignable'),
        'escape': ('''int[] saved = alias.Data; return saved[0];''', 'borrowed value cannot'),
        'reassign': ('''int[] saved; saved = alias.Data; return saved[0];''', 'borrowed value cannot'),
        'reference': ('''ref int item = ref alias.Data[0]; item = 10; return 0;''', 'mutable addressable storage'),
        'conditional_write': ('''(true ? alias.Data : alias.Data)[0] = 10; return 0;''', 'not assignable'),
        'coalesce_write': ('''(alias.Data ?? alias.Data)[0] = 10; return 0;''', 'not assignable'),
        'cast_write': ('''((int[])alias.Data)[0] = 10; return 0;''', 'not assignable'),
        'read': ('''Payload ordinary = new Payload(); ordinary.Data[0] = 42;
 return alias.Data[0] == 0 && ordinary.Data[0] == 42 ? 42 : 1;''', None),
        'nested_write': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>();
 borrowed Children nested = nestedOwner.Get(); nested.Data[0].Value = 10; return 0;''', 'not assignable'),
        'nested_escape': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>();
 borrowed Children nested = nestedOwner.Get(); Child saved = nested.Data[0]; return saved.Value;''',
                          'borrowed value cannot'),
    }
    with tempfile.TemporaryDirectory(prefix='nsc-readonly-array-') as temporary:
        root = Path(temporary)
        for source in sources:
            shutil.copyfile(source, root / source.name)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        for family in ('Local', 'Atomic'):
            for name, (body, expected) in cases.items():
                program = prefix + '''class Program { public static int Main() {
 HANDLE<Payload> owner = FACTORY.Create<Payload>(); borrowed Payload alias = owner.Get();
 ''' + body + '\n} }'
                program = program.replace('HANDLE', 'Shared' + family + 'Handle')
                program = program.replace('FACTORY', 'Shared' + family + 'Factory')
                (root / 'Main.ns').write_text(program, encoding='utf-8')
                result = run([compiler, '--check-project-method-bodies', project], root)
                output = result.stdout + result.stderr
                if expected:
                    assert result.returncode != 0 and expected in output, (family, name, output)
                    print(family, name, 'rejected', flush=True)
                else:
                    assert result.returncode == 0, (family, name, output)
                    if os.name == 'nt':
                        for level in ('O0', 'O2'):
                            exe = root / (family + level + '.exe')
                            build = run([compiler, '--emit-project-oop-exe', project, '-o', exe, '-' + level], root)
                            assert build.returncode == 0, (family, level, build.stdout + build.stderr)
                            execution = run([exe], root)
                            assert execution.returncode == 42, (family, level, execution.returncode)
                            print(family, name, level, 'exit=42', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
