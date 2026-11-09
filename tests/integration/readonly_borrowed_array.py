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
struct Carrier { public Payload Item; }
class Child { public int Value; }
class Children { public Child[] Data; public Children() { Data = new Child[1]; Data[0] = new Child(); } }
struct Box { public Child Item; public int Value; public Box() { Item = new Child(); Value = 42; } }
struct Scalar { public int Value; public Scalar() { Value = 42; } }
struct ScalarOuter { public Scalar Inner; public ScalarOuter() { Inner = new Scalar(); } }
class Boxes { public Box[] Data; public Boxes() { Data = new Box[1]; Data[0].Item = new Child(); Data[0].Value = 42; } }
class HandleHolder { public HANDLE<Payload> first; public HANDLE<Payload> second; }
'''
    cases = {
        'write': ('''alias.Data[0] = 10; return 0;''', 'not assignable'),
        'compound_write': ('''alias.Data[0] += 10; return 0;''', 'not assignable'),
        'postfix_write': ('''alias.Data[0]++; return 0;''', 'requires a modifiable'),
        'prefix_write': ('''++alias.Data[0]; return 0;''', 'requires a modifiable'),
        'ordinary_compound_write': ('''Payload ordinary = new Payload(); ordinary.Data[0] += 40;
 ordinary.Data[0]++; ++ordinary.Data[0]; return ordinary.Data[0];''', None),
        'escape': ('''int[] saved = alias.Data; return saved[0];''', 'borrowed value cannot'),
        'reassign': ('''int[] saved; saved = alias.Data; return saved[0];''', 'borrowed value cannot'),
        'reference': ('''ref int item = ref alias.Data[0]; item = 10; return 0;''', 'mutable addressable storage'),
        'tuple_alias': ('''(Payload, int) saved = (alias, 42); return 0;''', 'readonly borrowed value'),
        'tuple_direct': ('''(Payload, int) saved = (owner.Get(), 42); return 0;''', 'readonly borrowed value'),
        'tuple_nested': ('''((int, Payload), int) saved = ((1, alias), 42); return 0;''',
                         'readonly borrowed value'),
        'tuple_array': ('''(int[], int) saved = (alias.Data, 42); return 0;''', 'readonly borrowed value'),
        'tuple_aggregate_alias': ('''(Payload, int) saved = { alias, 42 }; return 0;''',
                                  'readonly borrowed value'),
        'struct_aggregate_alias': ('''Carrier saved = { alias }; return 0;''', 'readonly borrowed value'),
        'tuple_aggregate_nested': ('''((Payload, int), int) saved = { { alias, 1 }, 42 }; return 0;''',
                                   'readonly borrowed value'),
        'tuple_aggregate_scalar': ('''(int, int) saved = { alias.Data[0], 42 };
 var (item, number) = saved; return item + number;''', None),
        'struct_aggregate_ordinary': ('''Payload ordinary = new Payload(); Carrier saved = { ordinary };
 return saved.Item.Data[0] == 0 ? 42 : 1;''', None),
        'array_initializer_alias': ('''Payload[] saved = new Payload[] { alias }; return 0;''',
                                    'readonly borrowed value'),
        'array_initializer_direct': ('''Payload[] saved = new Payload[] { owner.Get() }; return 0;''',
                                     'readonly borrowed value'),
        'array_initializer_scalar': ('''int[] saved = new int[] { alias.Data[0], 42 };
 return saved[0] + saved[1];''', None),
        'array_initializer_ordinary': ('''Payload ordinary = new Payload();
 Payload[] saved = new Payload[] { ordinary }; return saved[0].Data[0] == 0 ? 42 : 1;''', None),
        'array_assignment_alias': ('''Payload[] saved = new Payload[1]; saved[0] = alias; return 0;''',
                                   'readonly borrowed value'),
        'array_assignment_direct': ('''Payload[] saved = new Payload[1]; saved[0] = owner.Get(); return 0;''',
                                    'readonly borrowed value'),
        'array_assignment_scalar': ('''int[] saved = new int[1]; saved[0] = alias.Data[0] + 42;
 return saved[0];''', None),
        'array_assignment_ordinary': ('''Payload[] saved = new Payload[1];
 saved[0] = new Payload(); return saved[0].Data[0] == 0 ? 42 : 1;''', None),
        'tuple_scalar': ('''var (item, number) = (alias.Data[0], 42); return item + number;''', None),
        'tuple_ordinary': ('''Payload ordinary = new Payload();
 (Payload, int) saved = (ordinary, 42); return 42;''', None),
        'tuple_owner_copy': ('''(HANDLE<Payload>, int) pair = (owner, 42); return 0;''', 'move-only'),
        'tuple_owner_aggregate_copy': ('''(HANDLE<Payload>, int) pair = { owner, 42 }; return 0;''',
                                       'move-only'),
        'tuple_owner_clone': ('''{ (HANDLE<Payload>, int) pair = (owner.Clone(), 42);
 if (owner.StrongCount() != 2l) return 1; } return owner.StrongCount() == 1l ? 42 : 2;''', None),
        'tuple_owner_aggregate_clone': ('''{ (HANDLE<Payload>, int) pair = { owner.Clone(), 42 };
 if (owner.StrongCount() != 2l) return 1; } return owner.StrongCount() == 1l ? 42 : 2;''', None),
        'conditional_owner_copy': ('''HANDLE<Payload> copy = true ? owner : owner; return 0;''',
                                   'move-only value cannot be copied by a conditional'),
        'tuple_owner_conditional_copy': ('''(HANDLE<Payload>, int) pair = (true ? owner : owner, 42);
 return 0;''', 'move-only value cannot be copied by a conditional'),
        'conditional_owner_clone': ('''{ HANDLE<Payload> copy = true ? owner.Clone() : owner.Clone();
 if (owner.StrongCount() != 2l) return 1; } return owner.StrongCount() == 1l ? 42 : 2;''', None),
        'conditional_owner_never': ('''{ HANDLE<Payload> copy = true ? owner.Clone() : panic("unexpected");
 if (owner.StrongCount() != 2l) return 1; } return owner.StrongCount() == 1l ? 42 : 2;''', None),
        'conditional_write': ('''(true ? alias.Data : alias.Data)[0] = 10; return 0;''', 'not assignable'),
        'mixed_conditional_write': ('''(true ? new int[1] : alias.Data)[0] = 10; return 0;''', 'not assignable'),
        'mixed_conditional_reverse': ('''(true ? alias.Data : new int[1])[0] = 10; return 0;''', 'not assignable'),
        'coalesce_write': ('''(alias.Data ?? alias.Data)[0] = 10; return 0;''', 'not assignable'),
        'mixed_coalesce_write': ('''(alias.Data ?? new int[1])[0] = 10; return 0;''', 'not assignable'),
        'cast_write': ('''((int[])alias.Data)[0] = 10; return 0;''', 'not assignable'),
        'read': ('''Payload ordinary = new Payload(); ordinary.Data[0] = 42;
 return alias.Data[0] == 0 && ordinary.Data[0] == 42 ? 42 : 1;''', None),
        'overwrite_borrowed_owner': ('''owner = FACTORY.Create<Payload>(); return alias.Data[0];''',
                                     'cannot overwrite owner'),
        'overwrite_after_borrow_scope': ('''HANDLE<Payload> other = FACTORY.Create<Payload>();
 { borrowed Payload temporary = other.Get(); if (temporary.Data[0] != 0) return 1; }
 other = FACTORY.Create<Payload>(); return other.Get().Data[0] == 0 ? 42 : 2;''', None),
        'overwrite_parameter_view': ('''return Work(FACTORY.Create<Payload>()); }
 public static int Work(HANDLE<Payload> value) { SharedBorrow<Payload> view = value.Borrow();
 value = FACTORY.Create<Payload>(); return view.Get().Data[0];''', 'cannot overwrite owner'),
        'move_parameter_view': ('''return Work(FACTORY.Create<Payload>()); }
 public static int Work(HANDLE<Payload> value) { SharedBorrow<Payload> view = value.Borrow();
 HANDLE<Payload> other = move(value); return view.Get().Data[0];''', 'cannot move'),
        'parameter_view_control': ('''return Work(FACTORY.Create<Payload>()); }
 public static int Work(HANDLE<Payload> value) {
 { SharedBorrow<Payload> view = value.Borrow(); if (view.Get().Data[0] != 0) return 1; }
 value = FACTORY.Create<Payload>(); return value.Get().Data[0] == 0 ? 42 : 2;''', None),
        'parameter_get_read': ('''return Work(FACTORY.Create<Payload>()); }
 public static int Work(HANDLE<Payload> value) { borrowed Payload item = value.Get();
 return item.Data[0] == 0 ? 42 : 1;''', None),
        'parameter_get_move': ('''return Work(FACTORY.Create<Payload>()); }
 public static int Work(HANDLE<Payload> value) { borrowed Payload item = value.Get();
 HANDLE<Payload> other = move(value); return item.Data[0];''', 'cannot move'),
        'parameter_get_replace': ('''return Work(FACTORY.Create<Payload>()); }
 public static int Work(HANDLE<Payload> value) { borrowed Payload item = value.Get();
 value = FACTORY.Create<Payload>(); return item.Data[0];''', 'cannot overwrite owner'),
        'parameter_view_get_read': ('''return Work(FACTORY.Create<Payload>()); }
 public static int Work(HANDLE<Payload> value) { SharedBorrow<Payload> view = value.Borrow();
 borrowed Payload item = view.Get(); return item.Data[0] == 0 ? 42 : 1;''', None),
        'parameter_view_get_move': ('''return Work(FACTORY.Create<Payload>()); }
 public static int Work(HANDLE<Payload> value) { SharedBorrow<Payload> view = value.Borrow();
 borrowed Payload item = view.Get(); HANDLE<Payload> other = move(value); return item.Data[0];''', 'cannot move'),
        'field_view_overwrite': ('''HandleHolder holder = new HandleHolder(); holder.first = FACTORY.Create<Payload>();
 SharedBorrow<Payload> view = holder.first.Borrow(); holder.first = FACTORY.Create<Payload>();
 return view.Get().Data[0];''', 'cannot overwrite owner field'),
        'field_view_alias_overwrite': ('''HandleHolder holder = new HandleHolder(); borrowed HandleHolder other = holder;
 holder.first = FACTORY.Create<Payload>(); SharedBorrow<Payload> view = holder.first.Borrow();
 other.first = FACTORY.Create<Payload>(); return view.Get().Data[0];''', 'cannot overwrite owner field'),
        'field_view_reassignment': ('''HandleHolder holder = new HandleHolder();
 holder.first = FACTORY.Create<Payload>(); holder.second = FACTORY.Create<Payload>();
 SharedBorrow<Payload> view = holder.first.Borrow(); view = holder.second.Borrow();
 holder.second = FACTORY.Create<Payload>(); return view.Get().Data[0];''', 'cannot overwrite owner field'),
        'field_view_other_field': ('''HandleHolder holder = new HandleHolder();
 holder.first = FACTORY.Create<Payload>(); holder.second = FACTORY.Create<Payload>();
 SharedBorrow<Payload> view = holder.first.Borrow(); holder.second = FACTORY.Create<Payload>();
 return view.Get().Data[0] == 0 && holder.second.Get().Data[0] == 0 ? 42 : 1;''', None),
        'field_view_alias_move': ('''HandleHolder holder = new HandleHolder(); borrowed HandleHolder other = holder;
 holder.first = FACTORY.Create<Payload>(); SharedBorrow<Payload> view = holder.first.Borrow();
 { HANDLE<Payload> moved = move(other.first); } return view.Get().Data[0];''', 'cannot partially move'),
        'field_view_move_after_scope': ('''HandleHolder holder = new HandleHolder();
 holder.first = FACTORY.Create<Payload>(); { SharedBorrow<Payload> view = holder.first.Borrow();
 if (view.Get().Data[0] != 0) return 1; } HANDLE<Payload> moved = move(holder.first);
 return moved.Get().Data[0] == 0 ? 42 : 2;''', None),
        'array_view_move': ('''HANDLE<Payload>[] values = new HANDLE<Payload>[1];
 values[0] = FACTORY.Create<Payload>(); SharedBorrow<Payload> view = values[0].Borrow();
 { HANDLE<Payload> moved = move(values[0]); } return view.Get().Data[0];''', 'cannot partially move'),
        'array_view_move_after_scope': ('''HANDLE<Payload>[] values = new HANDLE<Payload>[1];
 values[0] = FACTORY.Create<Payload>(); { SharedBorrow<Payload> view = values[0].Borrow();
 if (view.Get().Data[0] != 0) return 1; } HANDLE<Payload> moved = move(values[0]);
 return moved.Get().Data[0] == 0 ? 42 : 2;''', None),
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
        'direct_view_foreach_read': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>(); int sum = 0;
 foreach (Child child in nestedOwner.Borrow().Get().Data) { sum = sum + child.Value; }
 return sum == 0 ? 42 : 1;''', None),
        'direct_view_foreach_ref_read': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>(); int sum = 0;
 foreach (ref Child child in nestedOwner.Borrow().Get().Data) { sum = sum + child.Value; }
 return sum == 0 ? 42 : 1;''', None),
        'direct_view_foreach_escape': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>(); Child saved;
 foreach (Child child in nestedOwner.Borrow().Get().Data) { saved = child; } return 0;''', 'borrowed value cannot'),
        'direct_view_foreach_move': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>();
 foreach (Child child in nestedOwner.Borrow().Get().Data) { HANDLE<Children> moved = move(nestedOwner); }
 return 0;''', 'cannot move owner'),
        'named_view_foreach_read': ('''HANDLE<Children> nestedOwner = FACTORY.Create<Children>(); int sum = 0;
 SharedBorrow<Children> view = nestedOwner.Borrow();
 foreach (Child child in view.Get().Data) { sum = sum + child.Value; }
 return sum == 0 ? 42 : 1;''', None),
        'temporary_view_foreach': ('''foreach (Child child in FACTORY.Create<Children>().Borrow().Get().Data) {
 int value = child.Value; } return 0;''', 'temporary receiver'),
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
