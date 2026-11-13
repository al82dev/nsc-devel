"""Check source ownership and error flow for nested futures."""

import sys

import async_poll_call_exception as fixture


fixture.CASES = {
    'nested_future_source': '''public static async int FailAsync() throws Exception {
yield; Calls = Calls + 1; throw new Exception(7); }
public static async Future<int, Exception> Outer() { yield; return FailAsync(); }
public static async int Main() { int caught = 0; Future<int, Exception> inner = await Outer();
try { int ignored = await inner; return 1; } catch (Exception e) { caught = e.Id; }
return caught == 7 && Calls == 1 && Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 2; }''',
    'triple_future_source': '''public static async int FailAsync() throws Exception {
yield; Calls = Calls + 1; throw new Exception(7); }
public static async Future<int, Exception> Outer() { yield; return FailAsync(); }
public static async Future<Future<int, Exception> > OuterMost() { yield; return Outer(); }
public static async int Main() { int caught = 0;
Future<Future<int, Exception> > middle = await OuterMost();
Future<int, Exception> inner = await middle;
try { int ignored = await inner; return 1; } catch (Exception e) { caught = e.Id; }
return caught == 7 && Calls == 1 && Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 2; }''',
}

if __name__ == '__main__':
    sys.exit(fixture.main())
