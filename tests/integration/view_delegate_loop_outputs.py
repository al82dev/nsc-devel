"""Check view output origins across loops through delegate invocations."""
from pathlib import Path
import runpy
import sys

fixture = runpy.run_path(str(Path(__file__).with_name('view_ref_owner_replacement.py')))
fixture['main'].__globals__['TYPES'] += '''
delegate void SaveView(View input, ref View output);
delegate void SaveViewOut(View input, out View output);
'''
cases = fixture['CASES']
cases.clear()
owner = fixture['OWNER']
cases.update({
    'delegate_ref_escape': (False, owner + 'SaveView save = Sink.Save; View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); int i = 0; '
        'while (i < 2) { save(source, ref result); source = inner.Borrow(); i = i + 1; } '
        '} return result.Get();'),
    'delegate_out_escape': (False, owner + 'SaveViewOut save = Sink.SaveOut; View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); int i = 0; '
        'while (i < 2) { save(source, out result); source = inner.Borrow(); i = i + 1; } '
        '} return result.Get();'),
    'delegate_ref_alias_inner_owner': (False, 'SaveView save = Sink.Save; View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = inner.Borrow(); ref View alias = ref result; '
        'save(source, ref alias); } return result.Get();'),
    'delegate_for_escape': (False, owner + 'SaveView save = Sink.Save; View result = default(View); '
        '{ Owner inner; inner.Value = 7; View source = owner.Borrow(); '
        'for (int i = 0; i < 2; i = i + 1) { save(source, ref result); source = inner.Borrow(); } '
        '} return result.Get();'),
    'delegate_chain_escape': (False, owner + 'SaveView save = Sink.Save; View result = default(View); '
        '{ Owner inner; inner.Value = 7; View first = owner.Borrow(); View second = first; int i = 0; '
        'while (i < 3) { save(second, ref result); Sink.Save(first, ref second); '
        'first = inner.Borrow(); i = i + 1; } } return result.Get();'),
    'delegate_same_scope': (True, owner + 'Owner inner; inner.Value = 7; SaveView save = Sink.Save; '
        'View result = default(View); View source = owner.Borrow(); int i = 0; '
        'while (i < 2) { save(source, ref result); source = inner.Borrow(); i = i + 1; } '
        'return result.Get();'),
    'delegate_break_before_backedge': (True, owner + 'SaveView save = Sink.Save; '
        'View result = default(View); { Owner inner; inner.Value = 7; View source = owner.Borrow(); '
        'while (true) { save(source, ref result); source = inner.Borrow(); break; } } return result.Get();'),
})
sys.exit(fixture['main']())
