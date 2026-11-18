"""Reject local-owner views written through a ref field of unknown lifetime."""
from pathlib import Path
import runpy
import sys

fixture = runpy.run_path(str(Path(__file__).with_name('view_ref_owner_replacement.py')))
fixture['main'].__globals__['TYPES'] += '''
public borrowed struct Carrier { public View field; }
public borrowed struct OuterRef { public ref Carrier nested; }
'''
cases = fixture['CASES']
cases.clear()
cases.update({
    'reference_field_inner': (False, 'Carrier target = default(Carrier); OuterRef holder = new OuterRef(); '
        'holder.nested = ref target; { Owner inner; inner.Value = 7; View source = inner.Borrow(); '
        'Sink.Save(source, ref holder.nested.field); } return target.field.Get();'),
    'reference_field_out_inner': (False, 'Carrier target = default(Carrier); OuterRef holder = new OuterRef(); '
        'holder.nested = ref target; { Owner inner; inner.Value = 7; View source = inner.Borrow(); '
        'Sink.SaveOut(source, out holder.nested.field); } return target.field.Get();'),
    'reference_field_alias_inner': (False, 'Carrier target = default(Carrier); OuterRef holder = new OuterRef(); '
        'holder.nested = ref target; { Owner inner; inner.Value = 7; View source = inner.Borrow(); '
        'ref View alias = ref holder.nested.field; Sink.Save(source, ref alias); } return target.field.Get();'),
    'reference_field_reborrow_inner': (False, 'Carrier target = default(Carrier); OuterRef holder = new OuterRef(); '
        'holder.nested = ref target; { Owner inner; inner.Value = 7; View source = inner.Borrow(); '
        'ref View alias = ref holder.nested.field; ref View second = ref alias; '
        'Sink.Save(source, ref second); } return target.field.Get();'),
    # This safe case is rejected deliberately: the analysis cannot prove where the field points.
    'reference_field_same_scope_conservative': (False, 'Owner owner; owner.Value = 42; '
        'Carrier target = default(Carrier); OuterRef holder = new OuterRef(); holder.nested = ref target; '
        'View source = owner.Borrow(); Sink.Save(source, ref holder.nested.field); return target.field.Get();'),
    'value_field_same_scope': (True, 'Owner owner; owner.Value = 42; Carrier target = default(Carrier); '
        'View source = owner.Borrow(); Sink.Save(source, ref target.field); return target.field.Get();'),
    'reference_field_default': (True, 'Carrier target = default(Carrier); OuterRef holder = new OuterRef(); '
        'holder.nested = ref target; Sink.Save(default(View), ref holder.nested.field); return 0;'),
})
sys.exit(fixture['main']())
