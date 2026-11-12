"""Check an enclosing owner's lifetime across an exceptional async finally."""
import sys

import async_catch_owner_continuation as common


common.PREFIX = common.PREFIX.replace(
    'class Counter {',
    'class Owner { public static int Drops; public int Value; '
    'public Owner() { Value = 37; } ~Owner() { Drops = Drops + 1; } }\nclass Counter {')
common.CASES = {
    'catch_outer_owner_replacement_control': '''public static async int Fail() throws Exception {
 yield; throw new Exception(7);
} public static async int Work() throws Exception {
 raii Owner owner = new Owner();
 try { int value = await Fail(); }
 catch (Exception error) {
  int value = await Slow();
  if (Owner.Drops != 0 || owner.Value != 37 || error.Id != 7 || Exception.Drops != 0)
   return 1;
  throw new Exception(8);
 }
 return 0;
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) {
  if (error.Id == 8 && Owner.Drops == 1 && Exception.Drops == 1 && Exception.Log == 7)
   caught = 1;
 }
 return caught == 1 && Exception.Drops == 2 && Exception.Log == 78 ? 42 : 4;
}''',
    'catch_outer_owner_control': '''public static async int Fail() throws Exception {
 yield; throw new Exception(7);
} public static async int Work() throws Exception {
 raii Owner owner = new Owner();
 try { int value = await Fail(); }
 catch (Exception error) {
  if (Owner.Drops != 0 || owner.Value != 37) Counter.Log = 9;
  else Counter.Log = 1;
  int value = await Slow();
  if (Owner.Drops == 0 && owner.Value == 37) Counter.Log = Counter.Log * 10 + 2;
  else Counter.Log = 99;
  throw;
 }
 return 0;
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) { caught = error.Id; }
 if (Counter.Log != 12) return 10 + Counter.Log;
 if (caught != 7) return 30 + caught;
 if (Owner.Drops != 1) return 50 + Owner.Drops;
 return Exception.Drops == 1 ? 42 : 60 + Exception.Drops;
}''',
    'exceptional_finally_outer_owner': '''public static async int Fail() throws Exception {
 yield; throw new Exception(7);
} public static async int Work() throws Exception {
 raii Owner owner = new Owner();
 try { int value = await Fail(); }
 finally {
  if (Owner.Drops != 0 || owner.Value != 37) Counter.Log = 9;
  else Counter.Log = 1;
  int value = await Slow();
  if (Owner.Drops == 0 && owner.Value == 37) Counter.Log = Counter.Log * 10 + 2;
  else Counter.Log = 99;
 }
 return 0;
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) { caught = error.Id; }
 if (Counter.Log != 12) return 10 + Counter.Log;
 if (caught != 7) return 30 + caught;
 if (Owner.Drops != 1) return 50 + Owner.Drops;
 return Exception.Drops == 1 ? 42 : 60 + Exception.Drops;
}''',
    'exceptional_finally_outer_owner_lifetimes': '''public static async int Fail() throws Exception {
 yield; throw new Exception(7);
} public static async int Work() throws Exception {
 raii Owner owner = new Owner();
 try { int value = await Fail(); }
 finally { int value = await Slow(); }
 return 0;
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) { caught = error.Id; }
 if (caught == 7 && Owner.Drops == 1 && Exception.Drops == 1) return 42;
 return 60 + (Owner.Drops == 1 ? 1 : 0) + (Exception.Drops == 0 ? 2 : 0) + (caught == 0 ? 4 : 0);
}''',
}


if __name__ == '__main__':
    sys.exit(common.main())
