"""Check updates through borrowed aliases to fields from a NativeSharp library."""
import operator_increment_owned_field_binary as field


field.common.CONSUMER = '''class Program {
 static int Read(borrowed Box value) { return value.Value; }
 static int Update(borrowed Holder holder) { borrowed Holder alias = holder;
  borrowed Box view = ++alias.Item; borrowed Box copy = view; return Read(copy); }
 static int UpdateMixed(borrowed Holder holder) { raii Holder local = new Holder();
  borrowed Holder alias = local; alias = holder; ++alias.Item; return holder.Item.Value; }
 public static int Main() {
  { raii Holder holder = new Holder(); borrowed Holder alias = holder;
    { borrowed Box value = ++alias.Item;
      if (value.Value != 11 || Box.Count() != 1) return 1; }
    { raii Box old = alias.Item--;
      if (old.Value != 11 || holder.Item.Value != 10 || Box.Count() != 1) return 2; }
    if (Box.Count() != 2) return 3;
    int old = Read(alias.Item++);
    if (old != 10 || holder.Item.Value != 11 || Box.Count() != 3) return 4;
  }
  if (Box.Count() != 4) return 5;
  { raii Holder first = new Holder(); raii Holder second = new Holder();
    borrowed Holder alias = first; alias = second;
    { borrowed Box value = ++alias.Item;
      if (value.Value != 11 || first.Item.Value != 10 || Box.Count() != 5) return 6; }
    { raii Box old = alias.Item--;
      if (old.Value != 11 || second.Item.Value != 10 || Box.Count() != 5) return 7; }
    if (Box.Count() != 6) return 8;
  }
  if (Box.Count() != 8) return 9;
  { raii Holder holder = new Holder();
    if (Update(holder) != 11 || holder.Item.Value != 11 || Box.Count() != 9) return 10; }
  if (Box.Count() != 10) return 11;
  { raii Holder holder = new Holder();
    if (UpdateMixed(holder) != 11 || holder.Item.Value != 11 || Box.Count() != 12) return 12; }
  if (Box.Count() != 13) return 13;
  { raii Holder holder = new Holder(); borrowed Holder alias = holder;
    borrowed Box view = --alias.Item;
    if (view.Value != 9 || holder.Item.Value != 9 || Box.Count() != 14) return 14; }
  return Box.Count() == 15 ? 42 : 15;
 }
}
'''
field.common.ACTIVE_BORROW = '''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 public static int Main() { raii Holder first = new Holder(); raii Holder second = new Holder();
  borrowed Holder alias = first; alias = second;
  borrowed Box view = Identity(second.Item); ++alias.Item; return view.Value; }
}
'''
field.common.EXTRA_NEGATIVES = (
    ('decrement-view-return', '''class Program {
 static borrowed Box Update(borrowed Holder holder) { return --holder.Item; }
 public static int Main() { return 42; }
}''', 'borrowed field update cannot be returned'),
    ('reassigned-view-return', '''class Program {
 static borrowed Box Update(borrowed Holder holder, borrowed Box fallback) {
  borrowed Box view = ++holder.Item; borrowed Box alias = fallback;
  alias = view; return alias;
 }
 public static int Main() { return 42; }
}''', 'borrowed field update cannot be returned'),
    ('reassigned-view-chain-return', '''class Program {
 static borrowed Box Update(borrowed Holder holder, borrowed Box fallback) {
  borrowed Box view = ++holder.Item; borrowed Box alias = fallback;
  alias = view; borrowed Box next = alias; return next;
 }
 public static int Main() { return 42; }
}''', 'borrowed field update cannot be returned'),
)


if __name__ == '__main__':
    raise SystemExit(field.common.main())
