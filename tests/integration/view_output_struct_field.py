"""Check view outputs into fields of local value structs and exact ref aliases."""
from pathlib import Path
import runpy
import sys

fixture = runpy.run_path(str(Path(__file__).with_name('view_ref_owner_replacement.py')))
fixture['main'].__globals__['TYPES'] += '\npublic borrowed struct Carrier { public View field; }\n'
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
})
sys.exit(fixture['main']())
