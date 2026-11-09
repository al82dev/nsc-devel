import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(item) for item in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler, *sources = map(Path, sys.argv[1:])
    prefix = '''using NativeSharp.Core.Internal; using NativeSharp.Core.Threading;
namespace NativeSharp.Core { public class Exception { } }
class Payload { public static int Drops; public long Data; public Payload() { Data = 20l; }
 ~Payload() { Drops = Drops + 1; } }
struct Value { public static int Drops; public long Data; public Value() { Data = 22l; }
 ~Value() { Drops = Drops + 1; } }
class Resource { public static int Drops; ~Resource() { Drops = Drops + 1; } }
class Bad { public owned Resource item;
 public Bad() throws NativeSharp.Core.Exception { item = new Resource(); throw new NativeSharp.Core.Exception(); } }
struct BadValue { public owned Resource item;
 public BadValue() throws NativeSharp.Core.Exception { item = new Resource(); throw new NativeSharp.Core.Exception(); } }
class TrivialBase { public int data; }
class DerivedBad : TrivialBase { public owned Resource item;
 public DerivedBad() throws NativeSharp.Core.Exception { item = new Resource(); throw new NativeSharp.Core.Exception(); } }
'''
    safe = '''class Program { public static int Main() {
 SharedLocalWeak<Payload> weak = new SharedLocalWeak<Payload>(null, null);
 { SharedLocalHandle<Payload> owner = SharedLocalFactory.Create<Payload>();
   if (owner.IsEmpty() || owner.StrongCount() != 1l || owner.WeakCount() != 0l) return 1;
   weak = owner.Downgrade();
   if (owner.WeakCount() != 1l || weak.WeakCount() != 1l) return 10;
   { SharedLocalWeak<Payload> weakClone = weak.Clone();
     SharedLocalHandle<Payload> clone = owner.Clone();
     SharedLocalHandle<Payload> upgraded = weak.Upgrade();
     SharedBorrow<Payload> view = clone.Borrow();
   borrowed Payload alias = view.Get(); borrowed Payload direct = clone.Get();
     if (clone.StrongCount() != 3l || clone.WeakCount() != 2l || weakClone.WeakCount() != 2l ||
         alias.Data != 20l || direct.Data != 20l || clone.Get().Data != 20l || weakClone.Expired()) return 2;
   }
   if (owner.StrongCount() != 1l || Payload.Drops != 0 || weak.Expired()) return 3;
 }
 if (Payload.Drops != 1 || !weak.Expired() || weak.WeakCount() != 1l) return 4;
 { SharedLocalWeak<Payload> weakClone = weak.Clone();
   if (weakClone.WeakCount() != 2l || !weakClone.Expired()) return 11;
 }
 { SharedLocalHandle<Payload> expired = weak.Upgrade(); if (!expired.IsEmpty()) return 5; }
 { SharedLocalHandle<Value> owner = SharedLocalFactory.Create<Value>();
   SharedLocalWeak<Value> valueWeak = owner.Downgrade();
   SharedBorrow<Value> view = owner.Borrow();
   if (view.Get().Data != 22l || owner.Get().Data != 22l || valueWeak.Expired()) return 6;
 }
 if (Value.Drops != 1) return 7;
 int caught = 0;
 try { SharedLocalHandle<Bad> failed = SharedLocalFactory.CreateThrowing<Bad>(); }
 catch (NativeSharp.Core.Exception problem) { caught = caught + 1; }
 try { SharedLocalHandle<BadValue> failed = SharedLocalFactory.CreateThrowing<BadValue>(); }
 catch (NativeSharp.Core.Exception problem) { caught = caught + 1; }
 if (caught != 2 || Resource.Drops != 2) return 8;
 try { SharedLocalHandle<DerivedBad> failed = SharedLocalFactory.CreateThrowing<DerivedBad>(); }
 catch (NativeSharp.Core.Exception problem) { caught = caught + 1; }
 if (caught != 3 || Resource.Drops != 3) return 12;
 return 42;
} }'''
    copy = '''class Program { public static int Main() {
 SharedLocalHandle<Payload> owner = SharedLocalFactory.Create<Payload>();
 SharedLocalHandle<Payload> other = owner; return 0;
} }'''
    escape = '''class Program { public static int Main() {
 SharedBorrow<Payload> view;
 { SharedLocalHandle<Payload> owner = SharedLocalFactory.Create<Payload>(); view = owner.Borrow(); }
 return (int)view.Get().Data;
} }'''
    copy_weak = '''class Program { public static int Main() {
 SharedLocalHandle<Payload> owner = SharedLocalFactory.Create<Payload>();
 SharedLocalWeak<Payload> weak = owner.Downgrade(); SharedLocalWeak<Payload> other = weak; return 0;
} }'''
    escape_class = '''class Program { public static int Main() {
 Payload escaped; { SharedLocalHandle<Payload> owner = SharedLocalFactory.Create<Payload>();
 SharedBorrow<Payload> view = owner.Borrow(); escaped = view.Get(); }
 return (int)escaped.Data;
} }'''
    escape_direct = '''class Program { public static int Main() {
 Payload escaped; { SharedLocalHandle<Payload> owner = SharedLocalFactory.Create<Payload>();
 escaped = owner.Get(); } return (int)escaped.Data;
} }'''
    escape_direct_return = '''class Program { public static Payload Leak() {
 SharedLocalHandle<Payload> owner = SharedLocalFactory.Create<Payload>(); return owner.Get();
} public static int Main() { return 0; } }'''
    escape_direct_field = '''class Program { public static Payload Saved; public static int Main() {
 SharedLocalHandle<Payload> owner = SharedLocalFactory.Create<Payload>(); Saved = owner.Get(); return 0;
} }'''
    escape_direct_initializer = '''class Program { public static int Main() {
 SharedLocalHandle<Payload> owner = SharedLocalFactory.Create<Payload>();
 Payload escaped = owner.Get(); SharedLocalHandle<Payload> other = move(owner);
 return (int)escaped.Data;
} }'''
    escape_direct_argument = '''class Program { public static Payload Saved;
 public static void Store(Payload p) { Saved = p; }
 public static int Main() { SharedLocalHandle<Payload> owner = SharedLocalFactory.Create<Payload>();
 Store(owner.Get()); return 0; } }'''
    mutate_value = '''class Program { public static int Main() {
 SharedLocalHandle<Value> owner = SharedLocalFactory.Create<Value>();
 owner.Get().Data = 100l; return 0;
} }'''
    mutate_class = '''class Program { public static int Main() {
 SharedLocalHandle<Payload> owner = SharedLocalFactory.Create<Payload>();
 owner.Get().Data = 100l; return 0;
} }'''
    move_direct = '''class Program { public static int Main() {
 SharedLocalHandle<Payload> owner = SharedLocalFactory.Create<Payload>();
 borrowed Payload alias = owner.Get(); SharedLocalHandle<Payload> other = move(owner);
 return (int)alias.Data;
} }'''
    move_owner = '''class Program { public static int Main() {
 SharedLocalHandle<Payload> owner = SharedLocalFactory.Create<Payload>();
 SharedBorrow<Payload> view = owner.Borrow(); SharedLocalHandle<Payload> other = move(owner);
 return (int)view.Get().Data;
} }'''
    empty = '''class Program { public static int Main() {
 SharedLocalHandle<Payload> owner = new SharedLocalHandle<Payload>(null, null);
 SharedBorrow<Payload> view = owner.Borrow(); return (int)view.Get().Data;
} }'''
    empty_get = '''class Program { public static int Main() {
 SharedLocalHandle<Payload> owner = new SharedLocalHandle<Payload>(null, null);
 return (int)owner.Get().Data;
} }'''
    concurrency = '''class Concurrent {
 private static SharedAtomicHandle<Payload>* shared; private static long failures;
 public static void Work() { unsafe("atomic handle concurrency test") {
  int i = 0; while (i < 1000) {
   SharedAtomicHandle<Payload> clone = (*shared).Clone();
   if (clone.IsEmpty()) SharedAtomic.CompareExchange(ref failures, 1l, 0l);
   i = i + 1;
  }
 } }
 public static int Run() { unsafe("atomic handle concurrency test") {
  int before = Payload.Drops; failures = 0l;
  { SharedAtomicHandle<Payload> owner = SharedAtomicFactory.Create<Payload>(); shared = &owner;
    ThreadStart start = Work; raii Thread a = new Thread(start); raii Thread b = new Thread(start);
    raii Thread c = new Thread(start); Work(); a.Join(); b.Join(); c.Join();
    if (SharedAtomic.Load(ref failures) != 0l || owner.StrongCount() != 1l || Payload.Drops != before) return 1;
  }
  return Payload.Drops == before + 1 ? 42 : 2;
 } }
}'''
    with tempfile.TemporaryDirectory(prefix='nsc-shared-local-handles-') as temporary:
        root = Path(temporary)
        for source in sources:
            shutil.copyfile(source, root / source.name)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        for family in ('Local', 'Atomic'):
            for name, body, expected in (('safe', safe, None), ('empty', empty, None),
                                         ('empty_get', empty_get, None),
                                         ('copy', copy, 'move'), ('copy_weak', copy_weak, 'move'),
                                         ('escape', escape, 'longer-lived'),
                                         ('escape_class', escape_class, 'borrowed value cannot'),
                                         ('escape_direct', escape_direct, 'borrowed value cannot'),
                                         ('escape_direct_return', escape_direct_return, 'borrowed'),
                                         ('escape_direct_field', escape_direct_field, 'borrowed value cannot'),
                                         ('escape_direct_initializer', escape_direct_initializer, 'borrowed value cannot'),
                                         ('escape_direct_argument', escape_direct_argument, 'borrowed argument requires'),
                                         ('mutate_value', mutate_value, 'not assignable'),
                                         ('mutate_class', mutate_class, 'not assignable'),
                                         ('move_direct', move_direct, 'cannot move owner'),
                                         ('move_owner', move_owner, 'cannot move owner')):
                current_body = body
                if family == 'Atomic' and name == 'safe':
                    current_body = body.replace('return 42;', 'if (Concurrent.Run() != 42) return 9; return 42;')
                extra = concurrency if family == 'Atomic' and name == 'safe' else ''
                text = (prefix + extra + current_body).replace('SharedLocal', 'Shared' + family)
                (root / 'Main.ns').write_text(text, encoding='utf-8')
                result = run([compiler, '--check-project-method-bodies', project], root)
                output = result.stdout + result.stderr
                if expected:
                    assert result.returncode != 0 and expected in output, (family, name, output)
                    continue
                assert result.returncode == 0, (family, name, output)
                if os.name == 'nt':
                    for level in ('O0', 'O2'):
                        exe = root / (family + name + level + '.exe')
                        result = run([compiler, '--emit-project-oop-exe', project, '-o', exe,
                                      '--no-artifact-cache', '-' + level], root)
                        assert result.returncode == 0, (family, level, result.stdout + result.stderr)
                        result = run([exe], root)
                        print(f'{family} {name} {level} exit={result.returncode}', flush=True)
                        if name == 'safe':
                            assert result.returncode == 42, (family, level, result.returncode, result.stdout, result.stderr)
                        else:
                            # Default abort-profile panic is trap reason 14, surfaced as 128 + 14.
                            assert result.returncode == 142, (family, level, result.returncode, result.stdout, result.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
