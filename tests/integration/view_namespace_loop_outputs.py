"""Check view output origins across loops through namespace-function calls."""
from pathlib import Path
import runpy
import sys

fixture = runpy.run_path(str(Path(__file__).with_name('view_ref_owner_replacement.py')))
fixture['main'].__globals__['TYPES'] += '''
public borrowed struct Carrier { public View field; }
public borrowed struct Outer { public Carrier nested; }
void SaveGlobal(View input, ref View output) { output = input; }
void SaveGlobalOut(View input, out View output) { output = input; }
'''
cases = fixture['CASES']
cases.clear()
owner = fixture['OWNER']
cases.update({
    'namespace_ref_escape': (False, owner + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); int i = 0; '
        'while (i < 2) { SaveGlobal(source, ref result); source = inner.Borrow(); i = i + 1; } '
        '} return result.Get();'),
    'namespace_out_escape': (False, owner + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); int i = 0; '
        'while (i < 2) { SaveGlobalOut(source, out result); source = inner.Borrow(); i = i + 1; } '
        '} return result.Get();'),
    'namespace_ref_alias_inner_owner': (False, 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = inner.Borrow(); ref View alias = ref result; '
        'SaveGlobal(source, ref alias); } return result.Get();'),
    'namespace_struct_field_inner_owner': (False, 'Carrier holder = default(Carrier); '
        '{ Owner inner; inner.Value = 7; View source = inner.Borrow(); '
        'SaveGlobal(source, ref holder.field); } return holder.field.Get();'),
    'namespace_nested_field_inner_owner': (False, 'Outer holder = default(Outer); '
        '{ Owner inner; inner.Value = 7; View source = inner.Borrow(); '
        'SaveGlobal(source, ref holder.nested.field); } return holder.nested.field.Get();'),
    'namespace_struct_field_owner_replaced': (False, owner + 'Carrier holder = default(Carrier); '
        '{ View source = owner.Borrow(); SaveGlobal(source, ref holder.field); } '
        'ref Owner alias = ref owner; alias = default(Owner); return holder.field.Get();'),
    'namespace_for_escape': (False, owner + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); '
        'for (int i = 0; i < 2; i = i + 1) { SaveGlobal(source, ref result); source = inner.Borrow(); } '
        '} return result.Get();'),
    'namespace_method_chain_escape': (False, owner + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View first = owner.Borrow(); View second = first; int i = 0; '
        'while (i < 3) { SaveGlobal(second, ref result); Sink.Save(first, ref second); '
        'first = inner.Borrow(); i = i + 1; } } return result.Get();'),
    'namespace_same_scope': (True, owner + 'Owner inner; inner.Value = 7; View result = default(View); '
        'View source = owner.Borrow(); int i = 0; '
        'while (i < 2) { SaveGlobal(source, ref result); source = inner.Borrow(); i = i + 1; } '
        'return result.Get();'),
    'namespace_break_before_backedge': (True, owner + 'View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); '
        'while (true) { SaveGlobal(source, ref result); source = inner.Borrow(); break; } '
        '} return result.Get();'),
})
sys.exit(fixture['main']())
