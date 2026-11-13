"""A consumed value parameter cannot supply a view through an unknown ref field."""
from pathlib import Path
import runpy
import sys

fixture = runpy.run_path(str(Path(__file__).with_name('view_ref_owner_replacement.py')))
fixture['main'].__globals__['TYPES'] += '''
public borrowed struct Carrier { public View field; }
public borrowed struct OuterRef { public ref Carrier nested; }
class Leaker { public static void Save(Owner owner, ref OuterRef holder) {
 View source = owner.Borrow(); Sink.Save(source, ref holder.nested.field);
} }
'''
fixture['CASES'].clear()
fixture['CASES'].update({
    'consumed_parameter': (False, 'Carrier target = default(Carrier); OuterRef holder = new OuterRef(); '
        'holder.nested = ref target; Owner owner; owner.Value = 7; '
        'Leaker.Save(move(owner), ref holder); return target.field.Get();'),
})
sys.exit(fixture['main']())
