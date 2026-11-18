"""Verify owner-field updates through a borrowed local and their lifetime guards."""
from pathlib import Path
import subprocess
import sys
import tempfile

from operator_increment_owned_field import PREFIX


VALID = {
    'prefix': '''class Program { public static int Main() {
 { raii Holder holder = new Holder(); borrowed Holder alias = holder; ++alias.Item;
   if (holder.Item.Value != 11 || Box.Drops != 1) return 1; }
 return Box.Drops == 2 ? 42 : 2; } }''',
    'postfix': '''class Program { public static int Main() {
 { raii Holder holder = new Holder(); borrowed Holder alias = holder;
   { raii Box old = alias.Item--; if (old.Value != 10 || holder.Item.Value != 9) return 1; }
   if (Box.Drops != 1) return 2; }
 return Box.Drops == 2 ? 42 : 3; } }''',
    'prefix-argument': '''class Program {
 static int Read(borrowed Box value) { return value.Value; }
 public static int Main() { raii Holder holder = new Holder(); borrowed Holder alias = holder;
 return Read(++alias.Item) == 11 && holder.Item.Value == 11 ? 42 : 1; } }''',
    'prefix-local': '''class Program { public static int Main() {
 raii Holder holder = new Holder(); borrowed Holder alias = holder;
 { borrowed Box view = ++alias.Item; if (view.Value != 11) return 1; }
 holder.Item = new Box(20); return holder.Item.Value + 22; } }''',
    'decrement-prefix-local': '''class Program { public static int Main() {
 raii Holder holder = new Holder(); borrowed Holder alias = holder;
 { borrowed Box view = --alias.Item; if (view.Value != 9) return 1; }
 holder.Item = new Box(20); return holder.Item.Value + 22; } }''',
    'nested-alias': '''class Program { public static int Main() {
 { raii Holder holder = new Holder(); borrowed Holder first = holder; borrowed Holder second = first;
   ++second.Item; if (holder.Item.Value != 11 || Box.Drops != 1) return 1; }
 return Box.Drops == 2 ? 42 : 2; } }''',
    'same-owner-reassignment': '''class Program { public static int Main() {
 { raii Holder holder = new Holder(); borrowed Holder alias = holder; alias = holder;
   ++alias.Item; if (holder.Item.Value != 11 || Box.Drops != 1) return 1; }
 return Box.Drops == 2 ? 42 : 2; } }''',
    'multiple-possible-owners': '''class Program { public static int Main() {
 { raii Holder first = new Holder(); raii Holder second = new Holder();
   borrowed Holder alias = first; alias = second; ++alias.Item;
   if (first.Item.Value != 10 || second.Item.Value != 11 || Box.Drops != 1) return 1; }
 return Box.Drops == 3 ? 42 : 2; } }''',
    'multiple-owners-first-path': '''class Program { public static int Main() {
 { raii Holder first = new Holder(); raii Holder second = new Holder(); bool chooseSecond = false;
   borrowed Holder alias = first; if (chooseSecond) alias = second; ++alias.Item;
   if (first.Item.Value != 11 || second.Item.Value != 10 || Box.Drops != 1) return 1; }
 return Box.Drops == 3 ? 42 : 2; } }''',
    'multiple-owners-local-view': '''class Program { public static int Main() {
 { raii Holder first = new Holder(); raii Holder second = new Holder();
   borrowed Holder alias = first; alias = second;
   { borrowed Box view = ++alias.Item; if (view.Value != 11) return 1; }
   second.Item = new Box(20); if (second.Item.Value != 20) return 2; }
 return Box.Drops == 4 ? 42 : 3; } }''',
    'borrowed-parameter': '''class Program {
 static int Update(borrowed Holder holder) { ++holder.Item; return holder.Item.Value; }
 public static int Main() { raii Holder holder = new Holder(); return Update(holder) + 31; } }''',
    'borrowed-parameter-local': '''class Program {
 static int Update(borrowed Holder holder) {
  { borrowed Box view = ++holder.Item; if (view.Value != 11) return 1; }
  holder.Item = new Box(20); return holder.Item.Value; }
 public static int Main() { raii Holder holder = new Holder(); return Update(holder) + 22; } }''',
    'borrowed-parameter-local-read': '''class Program {
 static int Read(borrowed Box value) { return value.Value; }
 static int Update(borrowed Holder holder) { borrowed Box view = ++holder.Item; return Read(view); }
 public static int Main() { raii Holder holder = new Holder(); return Update(holder) + 31; } }''',
    'borrowed-parameter-view-alias-read': '''class Program {
 static int Read(borrowed Box value) { return value.Value; }
 static int Update(borrowed Holder holder) { borrowed Box view = ++holder.Item;
  borrowed Box alias = view; return Read(alias); }
 public static int Main() { raii Holder holder = new Holder(); return Update(holder) + 31; } }''',
    'unrelated-parameter-borrow-return': '''class Program {
 static borrowed Box Identity(borrowed Box value) { borrowed Box alias = value; return alias; }
 public static int Main() { raii Box value = new Box(10); return Identity(value).Value + 32; } }''',
    'borrowed-parameter-alias': '''class Program {
 static int Update(borrowed Holder holder) { borrowed Holder alias = holder;
  ++alias.Item; return holder.Item.Value; }
 public static int Main() { raii Holder holder = new Holder(); return Update(holder) + 31; } }''',
    'borrowed-parameter-alias-local': '''class Program {
 static int Update(borrowed Holder holder) { borrowed Holder alias = holder;
  { borrowed Box view = ++alias.Item; if (view.Value != 11) return 1; }
  holder.Item = new Box(20); return holder.Item.Value; }
 public static int Main() { raii Holder holder = new Holder(); return Update(holder) + 22; } }''',
    'mixed-owner-parameter-path': '''class Program {
 static int Update(borrowed Holder external) { raii Holder local = new Holder();
  borrowed Holder alias = local; alias = external; ++alias.Item;
  return local.Item.Value == 10 && external.Item.Value == 11 ? 42 : 1; }
 public static int Main() { raii Holder holder = new Holder(); return Update(holder); } }''',
    'mixed-owner-local-path': '''class Program {
 static int Update(borrowed Holder external) { raii Holder local = new Holder();
  borrowed Holder alias = external; alias = local; ++alias.Item;
  return local.Item.Value == 11 && external.Item.Value == 10 ? 42 : 1; }
 public static int Main() { raii Holder holder = new Holder(); return Update(holder); } }''',
    'mixed-owner-local-view': '''class Program {
 static int Update(borrowed Holder external) { raii Holder local = new Holder();
  borrowed Holder alias = local; alias = external;
  { borrowed Box view = ++alias.Item; if (view.Value != 11) return 1; }
  external.Item = new Box(20); return external.Item.Value == 20 ? 42 : 2; }
 public static int Main() { raii Holder holder = new Holder(); return Update(holder); } }''',
    'mixed-owner-reverse-local-view': '''class Program {
 static int Update(borrowed Holder external) { raii Holder local = new Holder();
  borrowed Holder alias = external; alias = local;
  { borrowed Box view = ++alias.Item; if (view.Value != 11) return 1; }
  local.Item = new Box(20); return local.Item.Value == 20 ? 42 : 2; }
 public static int Main() { raii Holder holder = new Holder(); return Update(holder); } }''',
}

INVALID = {
    'decrement-local-before-replacement': ('''class Program { public static int Main() {
 raii Holder holder = new Holder(); borrowed Holder alias = holder;
 borrowed Box view = --alias.Item; holder.Item = new Box(30); return view.Value; } }''',
        'cannot overwrite owner field while a borrowed view is alive'),
    'decrement-parameter-return': ('''class Program {
 static borrowed Box Update(borrowed Holder holder) { return --holder.Item; }
 public static int Main() { return 42; } }''', 'borrowed field update cannot be returned'),
    'view-before-update': ('''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 public static int Main() { raii Holder holder = new Holder(); borrowed Holder alias = holder;
 borrowed Box view = Identity(holder.Item); ++alias.Item; return view.Value; } }''',
        'cannot overwrite owner field while a borrowed view is alive'),
    'argument-before-update': ('''class Program {
 static int Both(borrowed Box first, borrowed Box second) { return first.Value + second.Value; }
 public static int Main() { raii Holder holder = new Holder(); borrowed Holder alias = holder;
 return Both(holder.Item, ++alias.Item); } }''',
        'cannot overwrite owner field while a borrowed view is alive'),
    'local-before-replacement': ('''class Program { public static int Main() {
 raii Holder holder = new Holder(); borrowed Holder alias = holder;
 borrowed Box view = ++alias.Item; holder.Item = new Box(30); return view.Value; } }''',
        'cannot overwrite owner field while a borrowed view is alive'),
    'local-before-alias-replacement': ('''class Program { public static int Main() {
 raii Holder holder = new Holder(); borrowed Holder alias = holder;
 borrowed Box view = ++alias.Item; alias.Item = new Box(30); return view.Value; } }''',
        'cannot overwrite owner field while a borrowed view is alive'),
    'reassign-alias-then-replace': ('''class Program { public static int Main() {
 raii Holder first = new Holder(); raii Holder second = new Holder(); borrowed Holder alias = first;
 borrowed Box view = ++alias.Item; alias = second; first.Item = new Box(30); return view.Value; } }''',
        'cannot overwrite owner field while a borrowed view is alive'),
    'update-before-mutation': ('''class Program {
 static int Both(borrowed Box first, int second) { return first.Value + second; }
 public static int Main() { raii Holder holder = new Holder(); borrowed Holder alias = holder;
 return Both(++alias.Item, holder.Reset()); } }''', 'borrowed field view is alive'),
    'update-before-alias-mutation': ('''class Program {
 static int Both(borrowed Box first, int second) { return first.Value + second; }
 public static int Main() { raii Holder holder = new Holder(); borrowed Holder alias = holder;
 return Both(++alias.Item, alias.Reset()); } }''', 'borrowed field view is alive'),
    'forwarded-return': ('''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static borrowed Box Escape() { raii Holder holder = new Holder(); borrowed Holder alias = holder;
 return Identity(++alias.Item); } public static int Main() { return 42; } }''',
        "borrowed return value outlives local owner 'holder'"),
    'multiple-owners-view': ('''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 public static int Main() { raii Holder first = new Holder(); raii Holder second = new Holder();
 borrowed Holder alias = first; alias = second; borrowed Box view = Identity(second.Item);
 ++alias.Item; return view.Value; } }''', 'cannot overwrite owner field while a borrowed view is alive'),
    'multiple-owners-forwarded-return': ('''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static borrowed Box Escape() { raii Holder first = new Holder(); raii Holder second = new Holder();
 borrowed Holder alias = first; alias = second; return Identity(++alias.Item); }
 public static int Main() { return 42; } }''', 'borrowed return value outlives local owner'),
    'multiple-owners-update-before-mutation': ('''class Program {
 static int Both(borrowed Box first, int second) { return first.Value + second; }
 public static int Main() { raii Holder first = new Holder(); raii Holder second = new Holder();
 borrowed Holder alias = first; alias = second; return Both(++alias.Item, second.Reset()); } }''',
        'borrowed field view is alive'),
    'multiple-owners-local-replace': ('''class Program { public static int Main() {
 raii Holder first = new Holder(); raii Holder second = new Holder(); borrowed Holder alias = first;
 alias = second; borrowed Box view = ++alias.Item; second.Item = new Box(30); return view.Value; } }''',
        'cannot overwrite owner field while a borrowed view is alive'),
    'multiple-owners-nested-mutation': ('''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static int Both(borrowed Box first, int second) { return first.Value + second; }
 public static int Main() { raii Holder first = new Holder(); raii Holder second = new Holder();
 borrowed Holder alias = first; alias = second; return Both(Identity(++alias.Item), second.Reset()); } }''',
        'borrowed field view is alive'),
    'multiple-owners-move': ('''class Program {
 static int Both(borrowed Box first, owned Holder second) { return first.Value + second.Count; }
 public static int Main() { raii Holder first = new Holder(); raii Holder second = new Holder();
 borrowed Holder alias = first; alias = second; return Both(++alias.Item, move(second)); } }''',
        "cannot move owner 'second' while a borrowed local is alive"),
    'multiple-owners-local-return': ('''class Program {
 static borrowed Box Escape() { raii Holder first = new Holder(); raii Holder second = new Holder();
 borrowed Holder alias = first; alias = second; borrowed Box view = ++alias.Item; return view; }
 public static int Main() { return 42; } }''', "outlives its owner 'first'"),
    'parameter-view-replace': ('''class Program {
 static int Update(borrowed Holder holder) { borrowed Box view = ++holder.Item;
  holder.Item = new Box(30); return view.Value; }
 public static int Main() { return 42; } }''',
        'cannot overwrite owner field while a borrowed view is alive'),
    'parameter-argument-mutation': ('''class Program {
 static int Both(borrowed Box first, int second) { return first.Value + second; }
 static int Update(borrowed Holder holder) { return Both(++holder.Item, holder.Reset()); }
 public static int Main() { return 42; } }''', 'borrowed field view is alive'),
    'parameter-argument-before-update': ('''class Program {
 static int Both(borrowed Box first, borrowed Box second) { return first.Value + second.Value; }
 static int Update(borrowed Holder holder) { return Both(holder.Item, ++holder.Item); }
 public static int Main() { return 42; } }''', 'cannot overwrite owner field while a borrowed view is alive'),
    'parameter-alias-mutation': ('''class Program {
 static int Both(borrowed Box first, int second) { return first.Value + second; }
 static int Update(borrowed Holder first, borrowed Holder second) { return Both(++first.Item, second.Reset()); }
 public static int Main() { return 42; } }''', 'borrowed field view is alive'),
    'parameter-return': ('''class Program {
 static borrowed Box Update(borrowed Holder holder) { return ++holder.Item; }
 public static int Main() { return 42; } }''', 'borrowed field update cannot be returned'),
    'parameter-return-forwarded': ('''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static borrowed Box Update(borrowed Holder holder) { return Identity(++holder.Item); }
 public static int Main() { return 42; } }''', 'borrowed field update cannot be returned'),
    'parameter-return-local': ('''class Program {
 static borrowed Box Update(borrowed Holder holder) { borrowed Box view = ++holder.Item; return view; }
 public static int Main() { return 42; } }''', 'borrowed field update cannot be returned'),
    'parameter-return-local-forwarded': ('''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static borrowed Box Update(borrowed Holder holder) {
  borrowed Box view = ++holder.Item; return Identity(view); }
 public static int Main() { return 42; } }''', 'borrowed field update cannot be returned'),
    'parameter-return-alias': ('''class Program {
 static borrowed Box Update(borrowed Holder holder) {
  borrowed Box view = ++holder.Item; borrowed Box alias = view; return alias; }
 public static int Main() { return 42; } }''', 'borrowed field update cannot be returned'),
    'parameter-return-reassigned-alias': ('''class Program {
 static borrowed Box Update(borrowed Holder holder, borrowed Box fallback) {
  borrowed Box view = ++holder.Item; borrowed Box alias = fallback; alias = view; return alias; }
 public static int Main() { return 42; } }''', 'borrowed field update cannot be returned'),
    'parameter-return-multiple-reassigned-aliases': ('''class Program {
 static borrowed Box Update(borrowed Holder holder, borrowed Box fallback) {
  borrowed Box view = ++holder.Item; borrowed Box first = fallback; borrowed Box second = fallback;
  first = view; second = first; return second; }
 public static int Main() { return 42; } }''', 'borrowed field update cannot be returned'),
    'parameter-alias-mutation-while-view': ('''class Program {
 static int Update(borrowed Holder holder) { borrowed Holder alias = holder;
  borrowed Box view = ++alias.Item; holder.Item = new Box(30); return view.Value; }
 public static int Main() { return 42; } }''', 'cannot overwrite owner field while a borrowed view is alive'),
    'parameter-alias-return': ('''class Program {
 static borrowed Box Update(borrowed Holder holder) { borrowed Holder alias = holder; return ++alias.Item; }
 public static int Main() { return 42; } }''', 'borrowed field update cannot be returned'),
    'parameter-alias-return-local': ('''class Program {
 static borrowed Box Update(borrowed Holder holder) { borrowed Holder alias = holder;
  borrowed Box view = ++alias.Item; borrowed Box other = view; return other; }
 public static int Main() { return 42; } }''', 'borrowed field update cannot be returned'),
    'parameter-alias-argument-mutation': ('''class Program {
 static int Both(borrowed Box first, int second) { return first.Value + second; }
 static int Update(borrowed Holder holder) { borrowed Holder alias = holder;
  return Both(++alias.Item, holder.Reset()); } public static int Main() { return 42; } }''',
        'borrowed field view is alive'),
    'parameter-field-return-forwarded': ('''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static borrowed Box Update(borrowed Holder holder) { return Identity(holder.Item); }
 public static int Main() { return 42; } }''', 'borrowed field cannot be returned without a field lifetime contract'),
    'parameter-alias-field-return-forwarded': ('''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static borrowed Box Update(borrowed Holder holder) { borrowed Holder alias = holder; return Identity(alias.Item); }
 public static int Main() { return 42; } }''', 'borrowed field cannot be returned without a field lifetime contract'),
    'parameter-alias-field-before-update': ('''class Program {
 static int Both(borrowed Box first, borrowed Box second) { return first.Value + second.Value; }
 static int Update(borrowed Holder holder) { borrowed Holder alias = holder;
  return Both(alias.Item, ++holder.Item); } public static int Main() { return 42; } }''',
        'cannot overwrite owner field while a borrowed view is alive'),
    'view-alias-reassigned-to-container': ('''class Node { public owned Node Item;
 public Node() { Item = null; }
 public static owned Node operator ++(Node value) { return new Node(); } }
 class Program { static int Mutate(borrowed Node value) { value.Item = new Node(); return 0; }
 static int Update(borrowed Node holder) { borrowed Node view = ++holder.Item;
  borrowed Node alias = view; alias = holder; return Mutate(alias); }
 public static int Main() { return 42; } }''', 'borrowed field view to a potentially mutating call'),
    'view-reassigned-to-container': ('''class Node { public owned Node Item;
 public Node() { Item = null; }
 public static owned Node operator ++(Node value) { return new Node(); } }
 class Program { static int Mutate(borrowed Node value) { value.Item = new Node(); return 0; }
 static int Update(borrowed Node holder) { borrowed Node view = ++holder.Item;
  borrowed Node still = view; view = holder; return Mutate(view) + (still.Item == null ? 0 : 1); }
 public static int Main() { return 42; } }''', 'borrowed field view to a potentially mutating call'),
    'mixed-owner-local-view': ('''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 static int Update(borrowed Holder external) { raii Holder local = new Holder();
  borrowed Holder alias = local; alias = external; borrowed Box view = Identity(local.Item);
  ++alias.Item; return view.Value; } public static int Main() { return 42; } }''',
        'cannot overwrite owner field while a borrowed view is alive'),
    'mixed-owner-return': ('''class Program {
 static borrowed Box Update(borrowed Holder external) { raii Holder local = new Holder();
  borrowed Holder alias = local; alias = external; return ++alias.Item; }
 public static int Main() { return 42; } }''', 'borrowed field update cannot be returned'),
    'mixed-owner-parameter-mutation': ('''class Program {
 static int Both(borrowed Box first, int second) { return first.Value + second; }
 static int Update(borrowed Holder external) { raii Holder local = new Holder();
  borrowed Holder alias = local; alias = external; return Both(++alias.Item, external.Reset()); }
 public static int Main() { return 42; } }''', 'borrowed field view is alive'),
    'mixed-owner-alias-field-before-update': ('''class Program {
 static int Both(borrowed Box first, borrowed Box second) { return first.Value + second.Value; }
 static int Update(borrowed Holder external) { raii Holder local = new Holder();
  borrowed Holder alias = local; alias = external; return Both(alias.Item, ++local.Item); }
 public static int Main() { return 42; } }''', 'cannot overwrite owner field while a borrowed view is alive'),
    'mixed-owner-reverse-local-return': ('''class Program {
 static borrowed Box Update(borrowed Holder external) { raii Holder local = new Holder();
  borrowed Holder alias = external; alias = local; borrowed Box view = ++alias.Item; return view; }
 public static int Main() { return 42; } }''', 'borrowed field update cannot be returned'),
}


def run(command, cwd):
    result = subprocess.run([str(item) for item in command], cwd=cwd, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline([str(item) for item in command]),
          'EXIT', result.returncode, result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-owned-field-alias-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>OwnedFieldAlias</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        for name, source in VALID.items():
            (root / 'Main.ns').write_text(PREFIX + source, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                              '-o', executable, '--no-artifact-cache'], root)
                if result.returncode:
                    failures.append((name, level, 'compile'))
                elif run([executable], root).returncode != 42:
                    failures.append((name, level, 'execute'))
        for name, (source, expected) in INVALID.items():
            (root / 'Main.ns').write_text(PREFIX + source, encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', project], root)
            if result.returncode == 0 or expected not in result.stdout + result.stderr:
                failures.append((name, 'missing expected lifetime diagnostic'))
    print('FAILURES', failures, flush=True)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
