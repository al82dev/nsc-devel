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
struct Box { public Child Item; public int Value; public Box() { Item = new Child(); Value = 42; } }
struct Scalar { public int Value; public Scalar() { Value = 42; } }
struct ScalarOuter { public Scalar Inner; public ScalarOuter() { Inner = new Scalar(); } }
class Boxes { public Box[] Data; public Boxes() { Data = new Box[1]; Data[0].Item = new Child(); Data[0].Value = 42; } }
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
        'nested_foreach_write': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>();
 borrowed Children nested = nestedOwner.Get(); foreach (Child child in nested.Data) { child.Value = 10; }
 return 0;''', 'not assignable'),
        'nested_foreach_read': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>();
 borrowed Children nested = nestedOwner.Get(); int sum = 0;
 foreach (Child child in nested.Data) { sum = sum + child.Value; }
 return sum == 0 ? 42 : 1;''', None),
        'direct_foreach_read': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>(); int sum = 0;
 foreach (Child child in nestedOwner.Get().Data) { sum = sum + child.Value; }
 return sum == 0 ? 42 : 1;''', None),
        'direct_foreach_ref_read': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>(); int sum = 0;
 foreach (ref Child child in nestedOwner.Get().Data) { sum = sum + child.Value; }
 return sum == 0 ? 42 : 1;''', None),
        'direct_foreach_escape': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>(); Child escaped;
 foreach (Child child in nestedOwner.Get().Data) { escaped = child; } return 0;''', 'borrowed value cannot'),
        'direct_foreach_move': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>();
 foreach (Child child in nestedOwner.Get().Data) { HANDLE<Children> moved = move(nestedOwner); }
 return 0;''', 'cannot move owner'),
        'temporary_foreach': ('''foreach (Child child in FACTORY.Create<Children>().Get().Data) {
 int value = child.Value; } return 0;''', 'borrow'),
        'nested_foreach_break': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>();
 borrowed Children nested = nestedOwner.Get(); int visits = 0;
 foreach (Child child in nested.Data) { visits = visits + 1; break; }
 return visits == 1 ? 42 : 1;''', None),
        'ordinary_foreach_break': ('''Children ordinary = new Children(); int visits = 0;
 foreach (Child child in ordinary.Data) { visits = visits + 1; break; }
 return visits == 1 ? 42 : 1;''', None),
        'ordinary_foreach_continue': ('''int[] values = new int[2]; int visits = 0;
 foreach (int value in values) { visits = visits + 1; continue; }
 return visits == 2 ? 42 : 1;''', None),
        'ordinary_foreach_conditional_break': ('''int[] values = new int[2]; int visits = 0;
 foreach (int value in values) { if (visits == 1) break; visits = visits + 1; }
 return visits == 1 ? 42 : 1;''', None),
        'nested_foreach_return': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>();
 borrowed Children nested = nestedOwner.Get();
 foreach (Child child in nested.Data) { return child.Value == 0 ? 42 : 1; } return 1;''', None),
        'nested_foreach_nested': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>();
 borrowed Children nested = nestedOwner.Get(); int visits = 0;
 foreach (Child child in nested.Data) { foreach (Child other in nested.Data) {
 visits = visits + 1 + child.Value + other.Value; } } return visits == 1 ? 42 : 1;''', None),
        'ordinary_foreach_read': ('''Children ordinary = new Children(); int visits = 0;
 foreach (Child child in ordinary.Data) { visits = visits + 1 + child.Value; }
 return visits == 1 ? 42 : 1;''', None),
        'scalar_foreach_read': ('''int sum = 0; foreach (int item in alias.Data) { sum = sum + item; }
 return sum == 0 ? 42 : 1;''', None),
        'scalar_foreach_ref_read': ('''int sum = 0; foreach (ref int item in alias.Data) { sum = sum + item; }
 return sum == 0 ? 42 : 1;''', None),
        'nested_foreach_escape': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>();
 borrowed Children nested = nestedOwner.Get(); Child escaped;
 foreach (Child child in nested.Data) { escaped = child; } return 0;''', 'borrowed value cannot'),
        'nested_foreach_move': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>();
 borrowed Children nested = nestedOwner.Get(); foreach (Child child in nested.Data) {
 HANDLE<Children> moved = move(nestedOwner); } return 0;''', 'cannot move owner'),
        'nested_foreach_ref_escape': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>();
 borrowed Children nested = nestedOwner.Get(); Child escaped;
 foreach (ref Child child in nested.Data) { escaped = child; } return 0;''', 'borrow'),
        'nested_foreach_ref_read': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>();
 borrowed Children nested = nestedOwner.Get(); int sum = 0;
 foreach (ref Child child in nested.Data) { sum = sum + child.Value; }
 return sum == 0 ? 42 : 1;''', None),
        'nested_foreach_ref_write': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>();
 borrowed Children nested = nestedOwner.Get();
 foreach (ref Child child in nested.Data) { child.Value = 10; } return 0;''', 'not assignable'),
        'nested_foreach_ref_move': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>();
 borrowed Children nested = nestedOwner.Get(); foreach (ref Child child in nested.Data) {
 HANDLE<Children> moved = move(nestedOwner); } return 0;''', 'cannot move owner'),
        'struct_foreach_ref_read': ('''HANDLE<Boxes> boxOwner = FACTORY.Create<Boxes>();
 borrowed Boxes boxes = boxOwner.Get(); int sum = 0;
 foreach (ref Box box in boxes.Data) { sum = sum + box.Value; }
 return sum == 42 ? 42 : 1;''', None),
        'struct_foreach_ref_escape': ('''HANDLE<Boxes> boxOwner = FACTORY.Create<Boxes>();
 borrowed Boxes boxes = boxOwner.Get(); Child escaped;
 foreach (ref Box box in boxes.Data) { escaped = box.Item; } return 0;''', 'borrowed value cannot'),
        'nested_foreach_ref_mut': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>();
 borrowed Children nested = nestedOwner.Get();
 foreach (ref mut Child child in nested.Data) { child.Value = 10; } return 0;''', 'mutable'),
        'struct_nested_escape': ('''HANDLE<Boxes> nestedOwner = FACTORY.Create<Boxes>();
 borrowed Boxes nested = nestedOwner.Get(); Child saved = nested.Data[0].Item; return saved.Value;''',
                                 'borrowed value cannot'),
        'struct_copy_escape': ('''HANDLE<Boxes> nestedOwner = FACTORY.Create<Boxes>();
 borrowed Boxes nested = nestedOwner.Get(); Box saved = nested.Data[0]; return saved.Item.Value;''',
                               'borrowed value cannot'),
        'struct_reassign_escape': ('''HANDLE<Boxes> nestedOwner = FACTORY.Create<Boxes>();
 borrowed Boxes nested = nestedOwner.Get(); Box saved; saved = nested.Data[0]; return saved.Item.Value;''',
                                   'borrowed value cannot'),
        'struct_cast_escape': ('''HANDLE<Boxes> nestedOwner = FACTORY.Create<Boxes>();
 borrowed Boxes nested = nestedOwner.Get(); Child saved = ((Box)nested.Data[0]).Item; return saved.Value;''',
                               'borrowed value cannot'),
        'struct_nested_read': ('''HANDLE<Boxes> nestedOwner = FACTORY.Create<Boxes>();
 borrowed Boxes nested = nestedOwner.Get(); return nested.Data[0].Value;''', None),
        'struct_payload_child_write': ('''HANDLE<Box> boxOwner = FACTORY.Create<Box>();
 boxOwner.Get().Item.Value = 10; return 0;''', 'not assignable'),
        'struct_payload_child_escape': ('''HANDLE<Box> boxOwner = FACTORY.Create<Box>();
 Child saved = boxOwner.Get().Item; return saved.Value;''', 'borrowed value cannot'),
        'struct_view_child_escape': ('''HANDLE<Box> boxOwner = FACTORY.Create<Box>();
 SharedBorrow<Box> view = boxOwner.Borrow(); Child saved = view.Get().Item; return saved.Value;''',
                                     'borrowed value cannot'),
        'struct_payload_child_mutation': ('''HANDLE<Box> boxOwner = FACTORY.Create<Box>();
 borrowed Child saved = boxOwner.Get().Item; saved.Value = 10; return 0;''',
                                          'borrowed local requires a direct live owner'),
        'struct_payload_copy': ('''HANDLE<Box> boxOwner = FACTORY.Create<Box>();
 Box copied = boxOwner.Get(); return copied.Value;''', 'borrowed value cannot'),
        'struct_payload_read': ('''HANDLE<Box> boxOwner = FACTORY.Create<Box>();
 return boxOwner.Get().Value;''', None),
        'scalar_payload_copy': ('''HANDLE<Scalar> scalarOwner = FACTORY.Create<Scalar>();
 Scalar copied = scalarOwner.Get(); return copied.Value;''', None),
        'nested_scalar_payload_copy': ('''HANDLE<ScalarOuter> scalarOwner = FACTORY.Create<ScalarOuter>();
 ScalarOuter copied = scalarOwner.Get(); return copied.Inner.Value;''', None),
    }
    selected = os.environ.get('NSC_READONLY_ARRAY_CASES')
    if selected:
        requested = set(selected.split(','))
        if requested.difference(cases):
            raise ValueError('unknown readonly array case')
        cases = {name: case for name, case in cases.items() if name in requested}
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
                    if (name.startswith('nested_foreach_') or name.startswith('direct_foreach_') or
                            name in ('temporary_foreach', 'struct_payload_child_mutation')):
                        print(output, flush=True)
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
