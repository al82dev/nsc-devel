"""Check view outputs into fields of local value structs and exact ref aliases."""
from pathlib import Path
import runpy
import sys

fixture = runpy.run_path(str(Path(__file__).with_name('view_ref_owner_replacement.py')))
fixture['main'].__globals__['TYPES'] += '''
public borrowed struct Carrier { public View field; }
public borrowed struct Outer { public Carrier nested; }
public borrowed struct Third { public Outer outer; }
'''
cases = fixture['CASES']
cases.clear()
cases.update({
    'direct_ref_inner': (False, 'Carrier holder = default(Carrier); '
        '{ Owner inner; inner.Value = 7; View source = inner.Borrow(); '
        'Sink.Save(source, ref holder.field); } return holder.field.Get();'),
    'direct_out_inner': (False, 'Carrier holder = default(Carrier); '
        '{ Owner inner; inner.Value = 7; View source = inner.Borrow(); '
        'Sink.SaveOut(source, out holder.field); } return holder.field.Get();'),
    'alias_ref_inner': (False, 'Carrier holder = default(Carrier); '
        '{ Owner inner; inner.Value = 7; View source = inner.Borrow(); ref View alias = ref holder.field; '
        'Sink.Save(source, ref alias); } return holder.field.Get();'),
    'alias_out_inner': (False, 'Carrier holder = default(Carrier); '
        '{ Owner inner; inner.Value = 7; View source = inner.Borrow(); ref View alias = ref holder.field; '
        'Sink.SaveOut(source, out alias); } return holder.field.Get();'),
    'reborrow_inner': (False, 'Carrier holder = default(Carrier); '
        '{ Owner inner; inner.Value = 7; View source = inner.Borrow(); ref View alias = ref holder.field; '
        'ref View second = ref alias; Sink.Save(source, ref second); } return holder.field.Get();'),
    'direct_same_scope': (True, 'Owner owner; owner.Value = 42; Carrier holder = default(Carrier); '
        'View source = owner.Borrow(); Sink.Save(source, ref holder.field); return holder.field.Get();'),
    'alias_same_scope': (True, 'Owner owner; owner.Value = 42; Carrier holder = default(Carrier); '
        'View source = owner.Borrow(); ref View alias = ref holder.field; '
        'Sink.Save(source, ref alias); return holder.field.Get();'),
    'owner_replaced_after_source_scope': (False, 'Owner owner; owner.Value = 42; '
        'Carrier holder = default(Carrier); { View source = owner.Borrow(); '
        'Sink.Save(source, ref holder.field); } ref Owner alias = ref owner; alias = default(Owner); '
        'return holder.field.Get();'),
    'nested_direct_inner': (False, 'Outer holder = default(Outer); '
        '{ Owner inner; inner.Value = 7; View source = inner.Borrow(); '
        'Sink.Save(source, ref holder.nested.field); } return holder.nested.field.Get();'),
    'nested_alias_inner': (False, 'Outer holder = default(Outer); '
        '{ Owner inner; inner.Value = 7; View source = inner.Borrow(); ref View alias = ref holder.nested.field; '
        'Sink.Save(source, ref alias); } return holder.nested.field.Get();'),
    'nested_reborrow_inner': (False, 'Outer holder = default(Outer); '
        '{ Owner inner; inner.Value = 7; View source = inner.Borrow(); ref View alias = ref holder.nested.field; '
        'ref View second = ref alias; Sink.Save(source, ref second); } return holder.nested.field.Get();'),
    'deep_field_inner': (False, 'Third holder = default(Third); '
        '{ Owner inner; inner.Value = 7; View source = inner.Borrow(); '
        'Sink.Save(source, ref holder.outer.nested.field); } return holder.outer.nested.field.Get();'),
    'nested_same_scope': (True, 'Owner owner; owner.Value = 42; Outer holder = default(Outer); '
        'View source = owner.Borrow(); Sink.Save(source, ref holder.nested.field); return holder.nested.field.Get();'),
    'nested_owner_replaced': (False, 'Owner owner; owner.Value = 42; Outer holder = default(Outer); '
        '{ View source = owner.Borrow(); Sink.Save(source, ref holder.nested.field); } '
        'ref Owner alias = ref owner; alias = default(Owner); return holder.nested.field.Get();'),
})
sys.exit(fixture['main']())
