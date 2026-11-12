"""Exercise owning field updates with user-defined increment and decrement."""
from pathlib import Path
import subprocess
import sys
import tempfile


PREFIX = '''class Box { public static int Drops; public int Value;
 public Box(int value) { Value = value; } ~Box() { Drops = Drops + 1; }
 public static owned Box operator ++(Box value) { return new Box(value.Value + 1); }
 public static owned Box operator --(Box value) { return new Box(value.Value - 1); } }
class Holder { public owned Box Item; public int Count;
 public Holder() { Item = new Box(10); Count = 40; }
 public int Reset() { Item = new Box(20); return 0; }
 public int ReadAndReset(borrowed Box value) { Reset(); return value.Value; } }
'''
CASES = {
    'local-control': '''class Program { public static int Main() {
 { raii Box value = new Box(10); ++value; if (value.Value != 11 || Box.Drops != 1) return 1; }
 return Box.Drops == 2 ? 42 : 2; } }''',
    'numeric-field-control': '''class Program { public static int Main() {
 raii Holder holder = new Holder(); ++holder.Count; return holder.Count + 1; } }''',
    'field-argument-control': '''class Program {
 static int Both(borrowed Box first, borrowed Box second) { return first.Value + second.Value; }
 public static int Main() { raii Holder holder = new Holder(); return Both(holder.Item, holder.Item) + 22; } }''',
    'field-argument-other-root': '''class Program {
 static int Both(borrowed Box first, int second) { return first.Value + second; }
 public static int Main() { raii Holder holder = new Holder(); raii Holder other = new Holder();
 return Both(holder.Item, other.Reset()) + 32; } }''',
    'field-borrowed-local-live': '''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 public static int Main() { raii Holder holder = new Holder(); borrowed Box view = Identity(holder.Item);
 return view.Value == 10 ? 42 : 1; } }''',
    'field-borrowed-local-live-namespace': '''borrowed Box Identity(borrowed Box value) { return value; }
int Main() { raii Holder holder = new Holder(); borrowed Box view = Identity(holder.Item);
 return view.Value == 10 ? 42 : 1; }''',
    'field-borrowed-local-finished': '''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 public static int Main() { raii Holder holder = new Holder(); { borrowed Box view = Identity(holder.Item);
 if (view.Value != 10) return 1; } holder.Item = new Box(30); return holder.Item.Value + 12; } }''',
    'prefix-owned-field': '''class Program { public static int Main() {
 { raii Holder holder = new Holder(); ++holder.Item;
   if (holder.Item.Value != 11 || Box.Drops != 1) return 1; }
 return Box.Drops == 2 ? 42 : 2; } }''',
    'prefix-owned-field-borrowed-argument': '''class Program {
 static int Read(borrowed Box value) { return value.Value; }
 public static int Main() { { raii Holder holder = new Holder(); int value = Read(++holder.Item);
 if (value != 11 || holder.Item.Value != 11 || Box.Drops != 1) return 1; }
 return Box.Drops == 2 ? 42 : 2; } }''',
    'prefix-owned-field-borrowed-local': '''class Program { public static int Main() {
 { raii Holder holder = new Holder(); { borrowed Box view = ++holder.Item;
 if (view.Value != 11 || Box.Drops != 1) return 1; }
 holder.Item = new Box(30); if (Box.Drops != 2) return 2; }
 return Box.Drops == 3 ? 42 : 3; } }''',
    'postfix-owned-field': '''class Program { public static int Main() {
 { raii Holder holder = new Holder(); { raii Box old = holder.Item--;
   if (old.Value != 10 || holder.Item.Value != 9 || Box.Drops != 0) return 1; }
   if (Box.Drops != 1) return 2; }
 return Box.Drops == 2 ? 42 : 3; } }''',
    'postfix-owned-field-discard': '''class Program { public static int Main() {
 { raii Holder holder = new Holder(); holder.Item++;
   if (holder.Item.Value != 11 || Box.Drops != 1) return 1; }
 return Box.Drops == 2 ? 42 : 2; } }''',
    'postfix-owned-field-borrowed-argument': '''class Program {
 static int Read(borrowed Box value) { return value.Value; }
 public static int Main() { { raii Holder holder = new Holder(); int old = Read(holder.Item++);
 if (old != 10 || holder.Item.Value != 11 || Box.Drops != 1) return 1; }
 return Box.Drops == 2 ? 42 : 2; } }''',
}
UNSAFE = {
    'prefix-field-overwrite-argument': '''class Program {
 static int Both(borrowed Box first, int second) { return first.Value + second; }
 public static int Main() { raii Holder holder = new Holder(); return Both(++holder.Item, holder.Reset()); } }''',
    'prefix-field-overwrite-namespace': '''int Both(borrowed Box first, int second) { return first.Value + second; }
int Main() { raii Holder holder = new Holder(); return Both(++holder.Item, holder.Reset()); }''',
    'prefix-field-overwrite-direct': '''class Program {
 static int Both(borrowed Box first, borrowed Box second) { return first.Value + second.Value; }
 public static int Main() { raii Holder holder = new Holder();
 return Both(++holder.Item, holder.Item = new Box(30)); } }''',
    'prefix-field-overwrite-nested': '''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static int Both(borrowed Box first, int second) { return first.Value + second; }
 public static int Main() { raii Holder holder = new Holder();
 return Both(Identity(++holder.Item), holder.Reset()); } }''',
    'prefix-field-return-escape': '''class Program {
 static borrowed Box Escape() { raii Holder holder = new Holder(); return ++holder.Item; }
 public static int Main() { return 42; } }''',
    'prefix-field-return-forwarded': '''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static borrowed Box Escape() { raii Holder holder = new Holder(); return Identity(++holder.Item); }
 public static int Main() { return 42; } }''',
    'prefix-field-return-forwarded-nested': '''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static borrowed Box Escape() { raii Holder holder = new Holder(); return Identity(Identity(++holder.Item)); }
 public static int Main() { return 42; } }''',
    'prefix-field-local-replace': '''class Program {
 public static int Main() { raii Holder holder = new Holder(); borrowed Box view = ++holder.Item;
 holder.Item = new Box(30); return view.Value; } }''',
    'field-borrowed-argument': '''class Program {
 static int Both(borrowed Box first, owned Box second) { return first.Value + second.Value; }
 public static int Main() { raii Holder holder = new Holder(); return Both(holder.Item, holder.Item++); } }''',
    'field-borrowed-namespace': '''int Both(borrowed Box first, owned Box second) { return first.Value + second.Value; }
int Main() { raii Holder holder = new Holder(); return Both(holder.Item, holder.Item++); }''',
    'field-borrowed-mutating-call': '''class Program {
 static int Both(borrowed Box first, int second) { return first.Value + second; }
 public static int Main() { raii Holder holder = new Holder(); return Both(holder.Item, holder.Reset()); } }''',
    'field-borrowed-move-root': '''class Program {
 static int Both(borrowed Box first, owned Holder second) { return first.Value + second.Count; }
 public static int Main() { raii Holder holder = new Holder(); return Both(holder.Item, move(holder)); } }''',
    'field-borrowed-move-root-namespace': '''int Both(borrowed Box first, owned Holder second) { return first.Value + second.Count; }
int Main() { raii Holder holder = new Holder(); return Both(holder.Item, move(holder)); }''',
    'field-borrowed-move-field': '''class Program {
 static int Both(borrowed Box first, owned Box second) { return first.Value + second.Value; }
 public static int Main() { raii Holder holder = new Holder(); return Both(holder.Item, move(holder.Item)); } }''',
    'field-borrowed-assign-field': '''class Program {
 static int Both(borrowed Box first, borrowed Box second) { return first.Value + second.Value; }
 public static int Main() { raii Holder holder = new Holder(); return Both(holder.Item, holder.Item = new Box(30)); } }''',
    'field-borrowed-return-escape': '''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static borrowed Box Escape() { raii Holder holder = new Holder(); return Identity(holder.Item); }
 public static int Main() { return 42; } }''',
    'field-borrowed-return-nested-escape': '''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static borrowed Box Escape() { raii Holder holder = new Holder(); return Identity(Identity(holder.Item)); }
 public static int Main() { return 42; } }''',
    'field-borrowed-static-escape': '''class Program {
 static Box Saved;
 static borrowed Box Identity(borrowed Box value) { return value; }
 public static int Main() { raii Holder holder = new Holder(); Saved = Identity(holder.Item); return 42; } }''',
    'field-borrowed-local-replace': '''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 public static int Main() { raii Holder holder = new Holder(); borrowed Box view = Identity(holder.Item);
 holder.Item = new Box(30); return view.Value; } }''',
    'field-borrowed-local-replace-namespace': '''borrowed Box Identity(borrowed Box value) { return value; }
int Main() { raii Holder holder = new Holder(); borrowed Box view = Identity(holder.Item);
 holder.Item = new Box(30); return view.Value; }''',
    'field-borrowed-local-move': '''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 public static int Main() { raii Holder holder = new Holder(); borrowed Box view = Identity(holder.Item);
 raii Box moved = move(holder.Item); return view.Value; } }''',
    'field-borrowed-local-root-move': '''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 public static int Main() { raii Holder holder = new Holder(); borrowed Box view = Identity(holder.Item);
 raii Holder moved = move(holder); return view.Value; } }''',
    'field-borrowed-local-alias-replace': '''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 public static int Main() { raii Holder holder = new Holder(); borrowed Holder alias = holder;
 borrowed Box view = Identity(holder.Item); alias.Item = new Box(30); return view.Value; } }''',
    'field-borrowed-local-mutate': '''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 public static int Main() { raii Holder holder = new Holder(); borrowed Box view = Identity(holder.Item);
 holder.Reset(); return view.Value; } }''',
    'field-borrowed-local-pass-container': '''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static int Mutate(borrowed Holder value) { return value.Reset(); }
 public static int Main() { raii Holder holder = new Holder(); borrowed Box view = Identity(holder.Item);
 Mutate(holder); return view.Value; } }''',
    'field-borrowed-local-reassign-source': '''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 public static int Main() { raii Holder first = new Holder(); raii Holder second = new Holder();
 borrowed Box view = Identity(first.Item); view = Identity(second.Item);
 second.Item = new Box(30); return view.Value; } }''',
    'field-borrowed-local-return': '''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static borrowed Box Escape() { raii Holder holder = new Holder(); borrowed Box view = Identity(holder.Item); return view; }
 public static int Main() { return 42; } }''',
    'field-borrowed-unqualified-escape': '''class Program {
 static Box Saved;
 static int Save(Box value) { Saved = value; return 0; }
 public static int Main() { raii Holder holder = new Holder(); return Save(holder.Item); } }''',
    'field-borrowed-container': '''class Program {
 static int Both(borrowed Box first, borrowed Holder second) { second.Reset(); return first.Value; }
 public static int Main() { raii Holder holder = new Holder(); return Both(holder.Item, holder); } }''',
    'field-borrowed-container-reverse': '''class Program {
 static int Both(borrowed Holder first, borrowed Box second) { first.Reset(); return second.Value; }
 public static int Main() { raii Holder holder = new Holder(); return Both(holder, holder.Item); } }''',
    'field-borrowed-container-namespace': '''int Both(borrowed Box first, borrowed Holder second) { second.Reset(); return first.Value; }
int Main() { raii Holder holder = new Holder(); return Both(holder.Item, holder); }''',
    'field-borrowed-container-reverse-namespace': '''int Both(borrowed Holder first, borrowed Box second) { first.Reset(); return second.Value; }
int Main() { raii Holder holder = new Holder(); return Both(holder, holder.Item); }''',
    'field-borrowed-container-alias': '''class Program {
 static int Both(borrowed Box first, borrowed Holder second) { second.Reset(); return first.Value; }
 public static int Main() { raii Holder holder = new Holder(); borrowed Holder alias = holder; return Both(holder.Item, alias); } }''',
    'field-borrowed-container-alias-namespace': '''int Both(borrowed Box first, borrowed Holder second) { second.Reset(); return first.Value; }
int Main() { raii Holder holder = new Holder(); borrowed Holder alias = holder; return Both(holder.Item, alias); }''',
    'field-borrowed-receiver': '''class Program { public static int Main() {
 raii Holder holder = new Holder(); return holder.ReadAndReset(holder.Item); } }''',
    'field-borrowed-receiver-alias': '''class Program { public static int Main() {
 raii Holder holder = new Holder(); borrowed Holder alias = holder; return alias.ReadAndReset(holder.Item); } }''',
    'field-borrowed-receiver-extension': '''extension Holder {
 public int ReadExtension(borrowed Box value) { this.Reset(); return value.Value; } }
class Program { public static int Main() { raii Holder holder = new Holder();
 return holder.ReadExtension(holder.Item); } }''',
    'field-borrowed-receiver-extension-alias': '''extension Holder {
 public int ReadExtension(borrowed Box value) { this.Reset(); return value.Value; } }
class Program { public static int Main() { raii Holder holder = new Holder(); borrowed Holder alias = holder;
 return alias.ReadExtension(holder.Item); } }''',
}
INDEPENDENT_MOVE = '''class Program {
 static int Both(borrowed Box first, owned Holder second) { return first.Value + second.Count; }
 public static int Main() { raii Holder holder = new Holder(); raii Holder other = new Holder();
 return Both(holder.Item, move(other)); } }'''
INDEPENDENT_FIELD_MOVE = '''class IndependentHolder { public owned Box Item; public owned Box Other;
 public IndependentHolder() { Item = new Box(10); Other = new Box(20); } }
class Program {
 static int Both(borrowed Box first, owned Box second) { return first.Value + second.Value; }
 public static int Main() { raii IndependentHolder holder = new IndependentHolder(); return Both(holder.Item, move(holder.Other)); } }'''
INDEPENDENT_FIELD_ASSIGN = '''class IndependentHolder { public owned Box Item; public owned Box Other;
 public IndependentHolder() { Item = new Box(10); Other = new Box(20); } }
class Program {
 static int Both(borrowed Box first, borrowed Box second) { return first.Value + second.Value; }
 public static int Main() { raii IndependentHolder holder = new IndependentHolder(); return Both(holder.Item, holder.Other = new Box(30)); } }'''
INDEPENDENT_CONTAINER = '''class Program {
 static int Both(borrowed Box first, borrowed Holder second) { return first.Value + second.Count; }
 public static int Main() { raii Holder holder = new Holder(); raii Holder other = new Holder();
 return Both(holder.Item, other); } }'''
INDEPENDENT_CONTAINER_ALIAS = '''class Program {
 static int Both(borrowed Box first, borrowed Holder second) { return first.Value + second.Count; }
 public static int Main() { raii Holder holder = new Holder(); raii Holder other = new Holder();
 borrowed Holder alias = other; return Both(holder.Item, alias); } }'''
INDEPENDENT_EXTENSION = '''extension Holder {
 public int ReadExtension(borrowed Box value) { this.Reset(); return value.Value; } }
class Program { public static int Main() { raii Holder holder = new Holder(); raii Holder other = new Holder();
 return other.ReadExtension(holder.Item); } }'''


def run(command, cwd):
    command = [str(value) for value in command]
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                            errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-owned-field-operator-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>OwnedFieldOperator</Name></PropertyGroup>'
            '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        for name, body in CASES.items():
            (root / 'Main.ns').write_text(PREFIX + body, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', executable,
                              '--no-artifact-cache'], root)
                if result.returncode:
                    failures.append((name, level, 'compile'))
                    continue
                result = run([executable], root)
                if result.returncode != 42:
                    failures.append((name, level, 'execute', result.returncode))
        for name, body in UNSAFE.items():
            (root / 'Main.ns').write_text(PREFIX + body, encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', project], root)
            if name == 'prefix-field-overwrite-direct':
                message = 'cannot overwrite owner field while a borrowed view is alive'
            elif name.startswith('prefix-field-overwrite-'):
                message = 'borrowed field view is alive'
            elif name.startswith('prefix-field-return-'):
                message = "borrowed return value outlives local owner 'holder'"
            elif name == 'prefix-field-local-replace':
                message = 'cannot overwrite owner field while a borrowed view is alive'
            elif name == 'field-borrowed-mutating-call':
                message = 'cannot call potentially mutating method while a borrowed field view is alive'
            elif name.startswith('field-borrowed-move-root'):
                message = "cannot move owner 'holder' while a borrowed local is alive"
            elif name == 'field-borrowed-move-field':
                message = 'cannot partially move owner field while a borrowed view is alive'
            elif name in ('field-borrowed-return-escape', 'field-borrowed-return-nested-escape'):
                message = "borrowed return value outlives local owner 'holder'"
            elif name == 'field-borrowed-static-escape':
                message = 'borrowed value cannot be stored in field'
            elif name.startswith('field-borrowed-local-replace'):
                message = 'cannot overwrite owner field while a borrowed view is alive'
            elif name == 'field-borrowed-local-move':
                message = 'cannot partially move owner field while a borrowed view is alive'
            elif name == 'field-borrowed-local-root-move':
                message = "cannot move owner 'holder' while a borrowed local is alive"
            elif name == 'field-borrowed-local-alias-replace':
                message = 'cannot overwrite owner field while a borrowed view is alive'
            elif name == 'field-borrowed-local-mutate':
                message = 'cannot call potentially mutating method while a borrowed field view is alive'
            elif name == 'field-borrowed-local-pass-container':
                message = 'cannot pass object with a borrowed field view to a potentially mutating call'
            elif name == 'field-borrowed-local-reassign-source':
                message = 'borrowed reassignment requires a named RAII owner'
            elif name == 'field-borrowed-local-return':
                message = "outlives its owner 'holder'"
            elif name == 'field-borrowed-unqualified-escape':
                message = 'borrowed argument requires a borrowed parameter'
            elif name.startswith('field-borrowed-container'):
                message = 'cannot pass object with a borrowed field view to a potentially mutating call'
            elif name.startswith('field-borrowed-receiver'):
                message = 'cannot call potentially mutating method while a borrowed field view is alive'
            else:
                message = 'cannot overwrite owner field while a borrowed view is alive'
            if result.returncode == 0 or message not in result.stdout + result.stderr:
                failures.append((name, 'missing field borrow protection'))
            if name.startswith('field-borrowed-receiver-extension'):
                output = result.stdout + result.stderr
                if output.count(message) != 1 or 'cannot pass object with a borrowed field view' in output:
                    failures.append((name, 'duplicate receiver diagnostic'))
        for name, body, expected in (
                ('independent-move', INDEPENDENT_MOVE, 50),
                ('independent-field-move', INDEPENDENT_FIELD_MOVE, 30),
                ('independent-container', INDEPENDENT_CONTAINER, 50),
                ('independent-container-alias', INDEPENDENT_CONTAINER_ALIAS, 50),
                ('independent-extension', INDEPENDENT_EXTENSION, 10)):
            (root / 'Main.ns').write_text(PREFIX + body, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', executable,
                              '--no-artifact-cache'], root)
                if result.returncode:
                    failures.append((name, level, 'compile'))
                    continue
                result = run([executable], root)
                if result.returncode != expected:
                    failures.append((name, level, 'execute', result.returncode))
        (root / 'Main.ns').write_text(PREFIX + INDEPENDENT_FIELD_ASSIGN, encoding='utf-8')
        result = run([compiler, '--check-project-method-bodies', project], root)
        output = result.stdout + result.stderr
        if 'cannot overwrite owner field while a borrowed view is alive' in output:
            failures.append(('independent-field-assign', 'unrelated field rejected'))
        if result.returncode == 0 or "borrowed parameter 'second' requires a checked owner or borrowed source" not in output:
            failures.append(('independent-field-assign', 'unexpected analysis result'))
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
