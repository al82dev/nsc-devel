"""Check forwarding from a borrowed parameter through a ref field."""
from pathlib import Path
import runpy
import sys

fixture = runpy.run_path(str(Path(__file__).with_name('view_ref_owner_replacement.py')))
fixture['main'].__globals__['TYPES'] += '''
public borrowed struct Carrier { public View field; }
public borrowed struct OuterRef { public ref Carrier nested; }
class Forwarder { public static void Save(ref Owner owner, ref OuterRef holder) {
 View source = owner.Borrow(); Sink.Save(source, ref holder.nested.field);
} }
'''
fixture['CASES'].clear()
fixture['CASES'].update({
    'borrowed_parameter_same_scope': (True, 'Carrier target = default(Carrier); OuterRef holder = new OuterRef(); '
        'holder.nested = ref target; Owner owner; owner.Value = 7; '
        'Forwarder.Save(ref owner, ref holder); return target.field.Get();'),
    'borrowed_parameter_escape': (False, 'Carrier target = default(Carrier); OuterRef holder = new OuterRef(); '
        'holder.nested = ref target; { Owner owner; owner.Value = 7; '
        'Forwarder.Save(ref owner, ref holder); } return target.field.Get();'),
})
sys.exit(fixture['main']())
