"""Check owned field updates from a separately compiled NativeSharp library."""
import operator_increment_owned_binary as common


common.PRODUCER += '''
public class Holder { public owned Box Item; public Holder() { Item = new Box(10); } }
'''
common.CONSUMER = '''class Program {
 static int Read(borrowed Box value) { return value.Value; }
 public static int Main() {
  { raii Holder holder = new Holder();
    { borrowed Box value = ++holder.Item;
      if (value.Value != 11 || Box.Count() != 1) return 1; }
    { raii Box old = holder.Item--;
      if (old.Value != 11 || holder.Item.Value != 10 || Box.Count() != 1) return 2; }
    if (Box.Count() != 2) return 3;
    int old = Read(holder.Item++);
    if (old != 10 || holder.Item.Value != 11 || Box.Count() != 3) return 4;
  }
  return Box.Count() == 4 ? 42 : 5;
 }
}
'''
common.ACTIVE_BORROW = '''class Program {
 static borrowed Box Identity(borrowed Box value) { return value; }
 public static int Main() { raii Holder holder = new Holder();
  borrowed Box view = Identity(holder.Item); ++holder.Item; return view.Value; }
}
'''


if __name__ == '__main__':
    raise SystemExit(common.main())
