"""Check view-owner replacement through ref aliases without executing negatives."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

TYPES = '''public borrowed struct View<lifetime a> {
 private borrowed[a] int* target;
 public View(ref[a] int value) { unsafe("view") { target = &value; } }
 public ref[this] int Get() { unsafe("view") { return ref *target; } }
}
struct Owner { public int Value; ~Owner() { }
 public View Borrow() { return new View(ref Value); } }
class Sink {
 public static void Save(View input, ref View output) { output = input; }
 public static void SaveOut(View input, out View output) { output = input; }
}
'''
OWNER = 'Owner owner; owner.Value = 42; '
DIRECT = 'View source = new View(ref owner.Value); '
RECEIVER = 'View source = owner.Borrow(); '
ALIAS = 'ref Owner original = ref owner; '
CASES = {
    'live_direct': (True, OWNER + DIRECT +
        '{ Owner owner; owner.Value = 7; View alias = source; return alias.Get(); }'),
    'live_receiver': (True, OWNER + RECEIVER +
        '{ Owner owner; owner.Value = 7; View alias = source; return alias.Get(); }'),
    'no_view': (True, OWNER + ALIAS + 'original = default(Owner); return 42;'),
    'after_view_scope': (True, OWNER +
        '{ View source = new View(ref owner.Value); int value = source.Get(); } ' +
        ALIAS + 'original = default(Owner); return 42;'),
    'before_view_declaration': (True, OWNER + ALIAS +
        'original = default(Owner); View source = new View(ref owner.Value); return source.Get();'),
    'branch_view_ended': (True, OWNER +
        'bool choose = true; if (choose) { View source = new View(ref owner.Value); int v = source.Get(); } '
        + ALIAS + 'original = default(Owner); return 42;'),
    'distinct_shadow_owner': (True, OWNER + DIRECT +
        '{ Owner owner; owner.Value = 7; ref Owner original = ref owner; original = default(Owner); } '
        'return source.Get();'),
    'unrelated_field': (True, 'Owner first; first.Value = 42; Owner second; second.Value = 7; '
        'View source = new View(ref first.Value); ref Owner original = ref second; '
        'original.Value = 8; return source.Get();'),
    'reborrow_no_view': (True, OWNER +
        'ref Owner first = ref owner; ref Owner second = ref first; ref Owner third = ref second; '
        'third = default(Owner); return 42;'),
    'direct_alias': (False, OWNER + DIRECT + ALIAS + 'original = default(Owner); return source.Get();'),
    'receiver_alias': (False, OWNER + RECEIVER + ALIAS + 'original = default(Owner); return source.Get();'),
    'direct_shadow_alias': (False, OWNER + DIRECT + ALIAS +
        '{ Owner owner; owner.Value = 7; View alias = source; original = default(Owner); return alias.Get(); }'),
    'receiver_shadow_alias': (False, OWNER + RECEIVER + ALIAS +
        '{ Owner owner; owner.Value = 7; View alias = source; original = default(Owner); return alias.Get(); }'),
    'reborrow': (False, OWNER + DIRECT +
        'ref Owner first = ref owner; ref Owner second = ref first; ref Owner third = ref second; '
        'third = default(Owner); return source.Get();'),
    'last_use_same_scope': (False, OWNER + DIRECT +
        'int value = source.Get(); ' + ALIAS + 'original = default(Owner); return value;'),
    'branch_owner_alias': (False, OWNER + DIRECT + ALIAS +
        'bool choose = true; if (choose) { original = default(Owner); } return source.Get();'),
    # These cases exercise producers and control flow; their semantic validity
    # and diagnostics must be checked before treating them as regression gates.
    'branch_distinct_shadow_owner': (True, OWNER + DIRECT +
        'bool choose = true; if (choose) { Owner owner; owner.Value = 7; '
        'ref Owner original = ref owner; original = default(Owner); } return source.Get();'),
    'loop_distinct_shadow_owner': (True, OWNER + DIRECT +
        'int i = 0; while (i < 2) { Owner owner; owner.Value = 7; '
        'ref Owner original = ref owner; original = default(Owner); i = i + 1; } return source.Get();'),
    'loop_live_owner_alias': (False, OWNER + DIRECT + ALIAS +
        'int i = 0; while (i < 2) { original = default(Owner); i = i + 1; } return source.Get();'),
    'reassigned_view_alias': (False, OWNER + 'View source = default(View); source = owner.Borrow(); ' +
        ALIAS + 'original = default(Owner); return source.Get();'),
    'ref_output_view_alias': (False, OWNER + 'View result = default(View); { ' + DIRECT +
        'Sink.Save(source, ref result); } ' + ALIAS + 'original = default(Owner); return result.Get();'),
    'out_output_view_alias': (False, OWNER + 'View result = default(View); { ' + DIRECT +
        'Sink.SaveOut(source, out result); } ' + ALIAS + 'original = default(Owner); return result.Get();'),
    'branch_assigned_view_alias': (False, OWNER + 'View source = default(View); bool choose = true; '
        'if (choose) { source = owner.Borrow(); } ' + ALIAS +
        'original = default(Owner); return source.Get();'),
    'direct_owner': (False, OWNER + DIRECT + 'owner = default(Owner); return source.Get();'),
    'receiver_owner': (False, OWNER + RECEIVER + 'owner = default(Owner); return source.Get();'),
    'ref_receiver_owner': (False, OWNER +
        'ref Owner original = ref owner; View source = original.Borrow(); '
        'original = default(Owner); return source.Get();'),
    'ref_receiver_reborrow': (False, OWNER +
        'ref Owner first = ref owner; ref Owner second = ref first; ref Owner third = ref second; '
        'View source = third.Borrow(); third = default(Owner); return source.Get();'),
    'ref_field_constructor_owner': (False, OWNER +
        'ref Owner original = ref owner; View source = new View(ref original.Value); '
        'original = default(Owner); return source.Get();'),
    'ref_receiver_output_owner': (False, OWNER +
        'ref Owner original = ref owner; View result = default(View); '
        '{ View source = original.Borrow(); Sink.Save(source, ref result); } '
        'original = default(Owner); return result.Get();'),
    'ref_receiver_reassigned_view': (False, OWNER +
        'ref Owner original = ref owner; View source = default(View); source = original.Borrow(); '
        'original = default(Owner); return source.Get();'),
    'ref_receiver_branch_view': (False, OWNER +
        'ref Owner original = ref owner; View source = default(View); bool choose = true; '
        'if (choose) { source = original.Borrow(); } original = default(Owner); return source.Get();'),
    'ref_receiver_shadow_owner': (False, OWNER +
        'ref Owner original = ref owner; View source = original.Borrow(); '
        '{ Owner owner; owner.Value = 7; original = default(Owner); } return source.Get();'),
    'ref_receiver_distinct_owner': (True,
        'Owner first; first.Value = 42; Owner second; second.Value = 7; '
        'ref Owner left = ref first; View source = left.Borrow(); '
        'ref Owner right = ref second; right = default(Owner); return source.Get();'),
    'ref_receiver_distinct_shadow': (True, OWNER +
        'ref Owner outer = ref owner; View source = outer.Borrow(); '
        '{ Owner owner; owner.Value = 7; ref Owner inner = ref owner; inner = default(Owner); } '
        'return source.Get();'),
    'ref_field_different_owner': (True,
        'Owner first; first.Value = 42; ref int field = ref first.Value; '
        'View source = new View(ref field); Owner second; second.Value = 7; '
        'ref Owner other = ref second; other = default(Owner); return source.Get();'),
    'view_ref_write_then_output_escape': (False, OWNER + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); '
        'ref View alias = ref source; alias = inner.Borrow(); '
        'Sink.Save(source, ref result); } return result.Get();'),
    'view_reborrow_write_then_output_escape': (False, OWNER + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); '
        'ref View alias = ref source; ref View second = ref alias; second = inner.Borrow(); '
        'Sink.Save(source, ref result); } return result.Get();'),
    'view_ref_write_then_output_same_scope': (True, OWNER + 'Owner other; other.Value = 7; '
        'View source = owner.Borrow(); ref View alias = ref source; alias = other.Borrow(); '
        'View result = default(View); Sink.Save(source, ref result); return result.Get();'),
    'view_output_ref_alias_inner_owner': (False, 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = inner.Borrow(); ref View alias = ref result; '
        'Sink.Save(source, ref alias); } return result.Get();'),
    'view_output_out_alias_inner_owner': (False, 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = inner.Borrow(); ref View alias = ref result; '
        'Sink.SaveOut(source, out alias); } return result.Get();'),
    'view_output_reborrow_alias_inner_owner': (False, 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = inner.Borrow(); ref View alias = ref result; '
        'ref View second = ref alias; Sink.Save(source, ref second); } return result.Get();'),
    'view_output_ref_alias_same_scope': (True, OWNER + 'View result = default(View); '
        'View source = owner.Borrow(); ref View alias = ref result; Sink.Save(source, ref alias); '
        'return result.Get();'),
    'view_output_ref_alias_owner_replace': (False, OWNER + 'View result = default(View); '
        'View source = owner.Borrow(); ref View alias = ref result; Sink.Save(source, ref alias); '
        'ref Owner original = ref owner; original = default(Owner); return alias.Get();'),
    'view_output_alias_after_source_scope': (False, OWNER + 'View result = default(View); '
        'ref View alias = ref result; { View source = owner.Borrow(); Sink.Save(source, ref alias); } '
        'ref Owner original = ref owner; original = default(Owner); return result.Get();'),
    'view_while_ref_backedge_escape': (False, OWNER + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); int i = 0; '
        'while (i < 2) { Sink.Save(source, ref result); source = inner.Borrow(); i = i + 1; } '
        '} return result.Get();'),
    'view_while_out_backedge_escape': (False, OWNER + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); int i = 0; '
        'while (i < 2) { Sink.SaveOut(source, out result); source = inner.Borrow(); i = i + 1; } '
        '} return result.Get();'),
    'view_while_chained_backedge_escape': (False, OWNER + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View first = owner.Borrow(); View second = first; int i = 0; '
        'while (i < 3) { Sink.Save(second, ref result); Sink.Save(first, ref second); '
        'first = inner.Borrow(); i = i + 1; } } return result.Get();'),
    'view_while_long_chain_backedge_escape': (False, OWNER + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View first = owner.Borrow(); View second = first; View third = first; '
        'int i = 0; while (i < 4) { Sink.Save(third, ref result); Sink.Save(second, ref third); '
        'Sink.Save(first, ref second); first = inner.Borrow(); i = i + 1; } } return result.Get();'),
    'view_while_assignment_chain_backedge_escape': (False, OWNER + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View first = owner.Borrow(); View second = first; View third = first; '
        'int i = 0; while (i < 4) { Sink.Save(third, ref result); third = second; '
        'second = first; first = inner.Borrow(); i = i + 1; } } return result.Get();'),
    'view_while_alias_backedge_escape': (False, OWNER + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); ref View alias = ref source; int i = 0; '
        'while (i < 2) { Sink.Save(source, ref result); alias = inner.Borrow(); i = i + 1; } '
        '} return result.Get();'),
    'view_do_while_backedge_escape': (False, OWNER + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); int i = 0; '
        'do { Sink.SaveOut(source, out result); source = inner.Borrow(); i = i + 1; } while (i < 2); '
        '} return result.Get();'),
    'view_while_backedge_same_scope': (True, OWNER + 'Owner inner; inner.Value = 7; '
        'View source = owner.Borrow(); View result = default(View); int i = 0; '
        'while (i < 2) { Sink.Save(source, ref result); source = inner.Borrow(); i = i + 1; } '
        'return result.Get();'),
    'view_while_long_chain_same_scope': (True, OWNER + 'Owner inner; inner.Value = 7; '
        'View result = default(View); View first = owner.Borrow(); View second = first; View third = first; '
        'int i = 0; while (i < 4) { Sink.Save(third, ref result); Sink.Save(second, ref third); '
        'Sink.Save(first, ref second); first = inner.Borrow(); i = i + 1; } return result.Get();'),
    'view_while_assignment_chain_same_scope': (True, OWNER + 'Owner inner; inner.Value = 7; '
        'View result = default(View); View first = owner.Borrow(); View second = first; View third = first; '
        'int i = 0; while (i < 4) { Sink.Save(third, ref result); third = second; '
        'second = first; first = inner.Borrow(); i = i + 1; } return result.Get();'),
    'view_while_break_before_backedge': (True, OWNER + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); '
        'while (true) { Sink.Save(source, ref result); source = inner.Borrow(); break; } '
        '} return result.Get();'),
    'view_for_ref_backedge_escape': (False, OWNER + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); '
        'for (int i = 0; i < 2; i = i + 1) { Sink.Save(source, ref result); source = inner.Borrow(); } '
        '} return result.Get();'),
    'view_for_out_backedge_escape': (False, OWNER + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); '
        'for (int i = 0; i < 2; i = i + 1) { Sink.SaveOut(source, out result); source = inner.Borrow(); } '
        '} return result.Get();'),
    'view_for_chained_backedge_escape': (False, OWNER + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View first = owner.Borrow(); View second = first; '
        'for (int i = 0; i < 3; i = i + 1) { Sink.Save(second, ref result); '
        'Sink.Save(first, ref second); first = inner.Borrow(); } } return result.Get();'),
    'view_for_backedge_same_scope': (True, OWNER + 'Owner inner; inner.Value = 7; '
        'View result = default(View); View source = owner.Borrow(); '
        'for (int i = 0; i < 2; i = i + 1) { Sink.Save(source, ref result); source = inner.Borrow(); } '
        'return result.Get();'),
    'view_for_break_before_backedge': (True, OWNER + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); '
        'for (int i = 0; i < 2; i = i + 1) { Sink.Save(source, ref result); '
        'source = inner.Borrow(); break; } } return result.Get();'),
    'view_foreach_ref_backedge_escape': (False, OWNER + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); int[] values = new int[2]; '
        'foreach (int value in values) { Sink.Save(source, ref result); source = inner.Borrow(); } '
        '} return result.Get();'),
    'view_foreach_backedge_same_scope': (True, OWNER + 'Owner inner; inner.Value = 7; '
        'View result = default(View); View source = owner.Borrow(); int[] values = new int[2]; '
        'foreach (int value in values) { Sink.Save(source, ref result); source = inner.Borrow(); } '
        'return result.Get();'),
}
for mode, method in (('ref', 'Save'), ('out', 'SaveOut')):
    for origin, initial in (('direct', DIRECT), ('receiver', RECEIVER)):
        CASES[origin + '_' + mode + '_escape'] = (False,
            'View result = default(View); { ' + OWNER + initial + 'View alias = source; '
            '{ Owner owner; owner.Value = 7; Sink.' + method + '(alias, ' + mode +
            ' result); } } return result.Get();')


def main():
    if os.name != 'nt' or len(sys.argv) != 2:
        print('usage: view_ref_owner_replacement.py <nsc>', file=sys.stderr)
        return 2
    compiler = Path(sys.argv[1]).resolve()
    temp_root = Path(os.environ['TEMP']).resolve()
    if compiler.drive.upper() != 'A:' or temp_root.drive.upper() != 'A:':
        raise ValueError('compiler and temporary directory must be on A:')
    repository = Path(__file__).resolve().parents[2]
    records = []
    with tempfile.TemporaryDirectory(prefix='nsc-view-ref-owner-', dir=temp_root) as directory:
        root = Path(directory)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>ViewRefOwner</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
        for name, (valid, body) in CASES.items():
            source = TYPES + 'class Program { public static int Main() { ' + body + ' } }'
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            command = [str(compiler), '--check-project-method-bodies', str(project)]
            result = subprocess.run(command, cwd=repository, capture_output=True,
                                    text=True, errors='replace', timeout=120)
            output = result.stdout + result.stderr
            expected_error = ('while a borrowed local is alive' in output or
                              'view output cannot outlive its possible owner' in output)
            passed = result.returncode == 0 if valid else result.returncode != 0 and expected_error
            records.append({'case': name, 'valid': valid, 'command': command, 'source': source,
                            'exit': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr,
                            'passed': passed})
            print(name, 'PASS' if passed else 'FAIL', result.returncode, flush=True)
    print(json.dumps(records, indent=2), flush=True)
    return int(not all(item['passed'] for item in records))


if __name__ == '__main__':
    sys.exit(main())
