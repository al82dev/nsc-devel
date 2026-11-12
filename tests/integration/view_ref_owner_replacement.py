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
    'direct_owner': (False, OWNER + DIRECT + 'owner = default(Owner); return source.Get();'),
    'receiver_owner': (False, OWNER + RECEIVER + 'owner = default(Owner); return source.Get();'),
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
