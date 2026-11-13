"""Require the replaced exception to be destroyed when finally throws."""
import os
import sys

import async_catch_owner_continuation as common


common.CASES = {
    'sync_preserve': '''public static int Work() throws Exception {
 try { throw new Exception(1); } finally { Counter.Log = Counter.Log * 10 + 3; }
} public static async int Main() { int caught = 0;
 try { int value = Work(); } catch (Exception error) {
  if (Exception.Drops == 0 && error.Id == 1) caught = 1;
 }
 return caught == 1 && Counter.Log == 3 && Exception.Drops == 1 && Exception.Log == 1 ? 42 : 5; }''',
    'sync_nested_preserve': '''public static int Work() throws Exception {
 try { try { throw new Exception(1); } finally { Counter.Log = Counter.Log * 10 + 3; } }
 finally { Counter.Log = Counter.Log * 10 + 4; }
} public static async int Main() { int caught = 0;
 try { int value = Work(); } catch (Exception error) { caught = error.Id; }
 return caught == 1 && Counter.Log == 34 && Exception.Drops == 1 && Exception.Log == 1 ? 42 : 6; }''',
    'sync_inner_catch': '''public static int Work() throws Exception {
 try { throw new Exception(1); } finally {
  try { throw new Exception(2); } catch (Exception inner) { Counter.Log = inner.Id; }
 }
} public static async int Main() { int caught = 0;
 try { int value = Work(); } catch (Exception error) { caught = error.Id; }
 return caught == 1 && Counter.Log == 2 && Exception.Drops == 2 && Exception.Log == 21 ? 42 : 7; }''',
    'sync_replace': '''public static int Work() throws Exception {
 try { throw new Exception(1); } finally { throw new Exception(2); }
} public static async int Main() { int caught = 0;
 try { int value = Work(); } catch (Exception error) { caught = error.Id; }
 if (caught != 2) return 40 + caught;
 if (Exception.Drops != 2) return 50 + Exception.Log;
 return Exception.Log == 12 ? 42 : 70 + Exception.Log; }''',
    'sync_call_replace': '''public static int Fail() throws Exception { throw new Exception(2); }
public static int Work() throws Exception {
 try { throw new Exception(1); } finally { int value = Fail(); }
} public static async int Main() { int caught = 0;
 try { int value = Work(); } catch (Exception error) { caught = error.Id; }
 return caught == 2 && Exception.Drops == 2 && Exception.Log == 12 ? 42 : 50 + Exception.Drops; }''',
    'async_ready_replace': '''public static async int Immediate() { return 7; }
public static async int Work() throws Exception {
 try { throw new Exception(1); } finally { int value = await Immediate(); throw new Exception(2); }
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) { caught = error.Id; }
 return caught == 2 && Exception.Drops == 2 && Exception.Log == 12 ? 42 : 20 + Exception.Drops; }''',
    'async_suspended_replace': '''public static async int Work() throws Exception {
 try { throw new Exception(1); } finally { int value = await Slow(); throw new Exception(2); }
} public static async int Main() { int caught = 0;
 try { int value = await Work(); } catch (Exception error) { caught = error.Id; }
 return caught == 2 && Exception.Drops == 2 && Exception.Log == 12 ? 42 : 30 + Exception.Drops; }''',
}


if __name__ == '__main__':
    selected = os.environ.get('NSC_ASYNC_REPLACEMENT_CASES')
    if selected:
        requested = set(selected.split(','))
        if requested.difference(common.CASES):
            raise ValueError('unknown async exception replacement case')
        common.CASES = {name: body for name, body in common.CASES.items() if name in requested}
    sys.exit(common.main())
