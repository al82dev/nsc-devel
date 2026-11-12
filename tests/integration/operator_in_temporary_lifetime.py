"""Check order and cleanup of temporary `in` operands of a user operator."""
from pathlib import Path
import subprocess
import sys
import tempfile


TYPES = '''namespace NativeSharp.Core { public class Exception { } }
class State { public static int Log; public static bool Raise; public static bool FailBuild; }
class Witness { public static int Seen; public Witness() { Seen = State.Log; } }
struct Tracked {
 public int Id;
 public Tracked(int id) throws NativeSharp.Core.Exception {
  Id = id; State.Log = State.Log * 10 + id;
  if (State.FailBuild && id == 2) throw new NativeSharp.Core.Exception();
 }
 ~Tracked() { State.Log = State.Log * 10 + Id + 3; }
 public static bool operator ==(in Tracked a, in Tracked b) throws NativeSharp.Core.Exception {
  State.Log = State.Log * 10 + 3;
  if (State.Raise) throw new NativeSharp.Core.Exception();
  return a.Id == 1 && b.Id == 2;
 }
 public static bool operator !=(in Tracked a, in Tracked b) throws NativeSharp.Core.Exception {
  return !(a == b);
 }
 public static Witness operator +(in Tracked a, in Tracked b) {
  State.Log = State.Log * 10 + 3; return null;
 }
}
struct Enclosed {
 public Tracked Item;
 public Enclosed(int id) throws NativeSharp.Core.Exception { Item = new Tracked(id); }
 public static bool operator ==(in Enclosed a, in Enclosed b) {
  State.Log = State.Log * 10 + 3;
  return a.Item.Id == 1 && b.Item.Id == 2;
 }
 public static bool operator !=(in Enclosed a, in Enclosed b) { return !(a == b); }
}
class Part {
 public int Id;
 public Part(int id) { Id = id; State.Log = State.Log * 10 + id; }
 ~Part() { State.Log = State.Log * 10 + Id + 3; }
}
struct EnclosedOwner {
 public owned Part Item;
 public EnclosedOwner(int id) throws NativeSharp.Core.Exception {
  Item = new Part(id);
  if (State.FailBuild && id == 2) throw new NativeSharp.Core.Exception();
 }
 public static bool operator ==(in EnclosedOwner a, in EnclosedOwner b) throws NativeSharp.Core.Exception {
  State.Log = State.Log * 10 + 3;
  if (State.Raise) throw new NativeSharp.Core.Exception();
  return a.Item.Id == 1 && b.Item.Id == 2;
 }
 public static bool operator !=(in EnclosedOwner a, in EnclosedOwner b) throws NativeSharp.Core.Exception {
  return !(a == b);
 }
}
'''

CASES = {
    'local-normal': '''int Main() { try { { Tracked a = new Tracked(1); Tracked b = new Tracked(2);
 bool result = a == b; if (!result || State.Log != 123) return 1; } }
 catch (NativeSharp.Core.Exception error) { return 6; }
 return State.Log == 12354 ? 42 : 2; }''',
    'nested-drop-local': '''int Main() { try { { Enclosed a = new Enclosed(1);
 Enclosed b = new Enclosed(2); bool result = a == b;
 if (!result || State.Log != 123) return 35; }
 return State.Log == 12354 ? 42 : 36;
 } catch (NativeSharp.Core.Exception error) { return 37; } }''',
    'nested-drop-temporary': '''int Main() { try {
 bool result = (new Enclosed(1) == new Enclosed(2)) && State.Log == 123;
 return result && State.Log == 12354 ? 42 : 38;
 } catch (NativeSharp.Core.Exception error) { return 39; } }''',
    'owned-field-local': '''int Main() { try { { EnclosedOwner a = new EnclosedOwner(1);
 EnclosedOwner b = new EnclosedOwner(2); bool result = a == b;
 if (!result || State.Log != 123) return 40; }
 return State.Log == 12354 ? 42 : 41;
 } catch (NativeSharp.Core.Exception error) { return 43; } }''',
    'owned-field-temporary': '''int Main() { try {
 bool result = (new EnclosedOwner(1) == new EnclosedOwner(2)) && State.Log == 123;
 return result && State.Log == 12354 ? 42 : 44;
 } catch (NativeSharp.Core.Exception error) { return 45; } }''',
    'owned-field-constructor-throw': '''int Main() { State.FailBuild = true; int caught = 0;
 try { bool result = new EnclosedOwner(1) == new EnclosedOwner(2); }
 catch (NativeSharp.Core.Exception error) { caught = 1; }
 return caught == 1 && State.Log == 1254 ? 42 : 46; }''',
    'owned-field-local-constructor-throw': '''int Main() { State.FailBuild = true; int caught = 0;
 try { EnclosedOwner a = new EnclosedOwner(1); EnclosedOwner b = new EnclosedOwner(2);
 bool result = a == b; } catch (NativeSharp.Core.Exception error) { caught = 1; }
 return caught == 1 && State.Log == 1254 ? 42 : 47; }''',
    'owned-field-operator-throw': '''int Main() { State.Raise = true; int caught = 0;
 try { bool result = new EnclosedOwner(1) == new EnclosedOwner(2); }
 catch (NativeSharp.Core.Exception error) { caught = 1; }
 return caught == 1 && State.Log == 12354 ? 42 : 48; }''',
    'owned-field-skipped-branch': '''int Main() { try { bool evaluate = false;
 bool result = evaluate && (new EnclosedOwner(1) == new EnclosedOwner(2));
 return !result && State.Log == 0 ? 42 : 49;
 } catch (NativeSharp.Core.Exception error) { return 50; } }''',
    'temporary-normal': '''int Main() { try { bool result = new Tracked(1) == new Tracked(2);
 return result && State.Log == 12354 ? 42 : 3; }
 catch (NativeSharp.Core.Exception error) { return 7; } }''',
    'temporary-full-expression': '''int Main() { try {
 bool result = (new Tracked(1) == new Tracked(2)) && State.Log == 123;
 return result && State.Log == 12354 ? 42 : 13;
 } catch (NativeSharp.Core.Exception error) { return 14; } }''',
    'temporary-coalescing': '''int Main() { try {
 Witness result = (new Tracked(1) + new Tracked(2)) ?? new Witness();
 return result != null && Witness.Seen == 123 && State.Log == 12354 ? 42 : 51;
 } catch (NativeSharp.Core.Exception error) { return 52; } }''',
    'temporary-coalescing-skipped': '''int Main() { try {
 Witness existing = new Witness();
 Witness result = existing ?? (new Tracked(1) + new Tracked(2));
 return result != null && State.Log == 0 ? 42 : 53;
 } catch (NativeSharp.Core.Exception error) { return 54; } }''',
    'temporary-coalescing-taken': '''int Main() { try {
 Witness missing = null;
 Witness result = missing ?? (new Tracked(1) + new Tracked(2));
 return result == null && State.Log == 12354 ? 42 : 55;
 } catch (NativeSharp.Core.Exception error) { return 56; } }''',
    'temporary-conditional-test': '''int Main() { try {
 int result = (new Tracked(1) == new Tracked(2)) ? (State.Log == 123 ? 1 : 0) : 0;
 return result == 1 && State.Log == 12354 ? 42 : 27;
 } catch (NativeSharp.Core.Exception error) { return 28; } }''',
    'temporary-conditional-taken': '''int Main() { try {
 bool choice = true;
 bool result = (choice ? (new Tracked(1) == new Tracked(2)) : false) && State.Log == 123;
 return result && State.Log == 12354 ? 42 : 29;
 } catch (NativeSharp.Core.Exception error) { return 30; } }''',
    'temporary-conditional-skipped': '''int Main() { try {
 bool choice = false;
 bool result = (choice ? (new Tracked(1) == new Tracked(2)) : true) && State.Log == 0;
 return result && State.Log == 0 ? 42 : 31;
 } catch (NativeSharp.Core.Exception error) { return 32; } }''',
    'temporary-conditional-both-arms': '''int Main() { try {
 bool choice = false;
 bool result = (choice ? (new Tracked(1) == new Tracked(2)) :
                         (new Tracked(1) == new Tracked(2))) && State.Log == 123;
 return result && State.Log == 12354 ? 42 : 33;
 } catch (NativeSharp.Core.Exception error) { return 34; } }''',
    'two-calls-full-expression': '''int Main() { try {
 bool result = (new Tracked(1) == new Tracked(2)) &&
               (new Tracked(1) == new Tracked(2)) && State.Log == 123123;
 return result && State.Log == 1231235454 ? 42 : 23;
 } catch (NativeSharp.Core.Exception error) { return 24; } }''',
    'repeated-full-expression': '''int Main() { try {
 for (int i = 0; i < 2; i = i + 1) {
  State.Log = 0;
  bool result = (new Tracked(1) == new Tracked(2)) && State.Log == 123;
  if (!result || State.Log != 12354) return 25;
 }
 return 42;
 } catch (NativeSharp.Core.Exception error) { return 26; } }''',
    'temporary-skipped-branch': '''int Main() { try {
 bool evaluate = false;
 bool result = evaluate && (new Tracked(1) == new Tracked(2));
 return !result && State.Log == 0 ? 42 : 16;
 } catch (NativeSharp.Core.Exception error) { return 17; } }''',
    'temporary-taken-branch': '''int Main() { try {
 bool evaluate = true;
 bool result = evaluate && (new Tracked(1) == new Tracked(2));
 return result && State.Log == 12354 ? 42 : 18;
 } catch (NativeSharp.Core.Exception error) { return 19; } }''',
    'local-skipped-branch': '''int Main() { try { { Tracked a = new Tracked(1);
 Tracked b = new Tracked(2); bool evaluate = false;
 bool result = evaluate && (a == b);
 if (result || State.Log != 12) return 20; }
 return State.Log == 1254 ? 42 : 21;
 } catch (NativeSharp.Core.Exception error) { return 22; } }''',
    'one-temporary-normal': '''int Main() { try { { Tracked a = new Tracked(1);
 bool result = a == new Tracked(2); if (!result || State.Log != 1235) return 8; }
 return State.Log == 12354 ? 42 : 9; }
 catch (NativeSharp.Core.Exception error) { return 10; } }''',
    'local-throw': '''int Main() { State.Raise = true; int caught = 0;
 try { Tracked a = new Tracked(1); Tracked b = new Tracked(2); bool result = a == b; }
 catch (NativeSharp.Core.Exception error) { caught = 1; }
 return caught == 1 && State.Log == 12354 ? 42 : 4; }''',
    'temporary-throw': '''int Main() { State.Raise = true; int caught = 0;
 try { bool result = new Tracked(1) == new Tracked(2); }
 catch (NativeSharp.Core.Exception error) { caught = 1; }
 return caught == 1 && State.Log == 12354 ? 42 : 5; }''',
    'one-temporary-throw': '''int Main() { State.Raise = true; int caught = 0;
 try { Tracked a = new Tracked(1); bool result = a == new Tracked(2); }
 catch (NativeSharp.Core.Exception error) { caught = 1; }
 return caught == 1 && State.Log == 12354 ? 42 : 15; }''',
    'local-constructor-throw': '''int Main() { State.FailBuild = true; int caught = 0;
 try { Tracked a = new Tracked(1); Tracked b = new Tracked(2); bool result = a == b; }
 catch (NativeSharp.Core.Exception error) { caught = 1; }
 return caught == 1 && State.Log == 124 ? 42 : 11; }''',
    'temporary-constructor-throw': '''int Main() { State.FailBuild = true; int caught = 0;
 try { bool result = new Tracked(1) == new Tracked(2); }
 catch (NativeSharp.Core.Exception error) { caught = 1; }
 return caught == 1 && State.Log == 124 ? 42 : 12; }''',
}


def main():
    compiler = Path(sys.argv[1]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-in-operator-lifetime-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>InOperatorLifetime</Name></PropertyGroup>'
            '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
        for name, body in CASES.items():
            (root / 'Main.ns').write_text(TYPES + body, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                        errors='replace', timeout=120)
                print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                      '\n', result.stdout, result.stderr, flush=True)
                if result.returncode:
                    failures.append((name, level, 'compile'))
                    continue
                run = subprocess.run([str(executable)], cwd=root, capture_output=True,
                                     text=True, errors='replace', timeout=120)
                print('EXECUTE', name, level, 'EXIT', run.returncode, run.stdout, run.stderr, flush=True)
                if run.returncode != 42:
                    failures.append((name, level, 'execute', run.returncode))
    if failures:
        print('FAIL', failures, file=sys.stderr)
        return 1
    print('operator_in_temporary_lifetime: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
