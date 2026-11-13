"""Check nested Future value and error identities across binary boundaries."""

import sys

import async_poll_external_consumer as fixture


fixture.PRODUCER = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public static int Builds; public static int Drops;
public int Id; public Exception(int id) { Id = id; Builds = Builds + 1; }
~Exception() { Drops = Drops + 1; } } }
public class Service { public static int Calls;
public static async int Fail() throws Exception { yield; Calls = Calls + 1; throw new Exception(7); }
public static async int Plain() { yield; return 5; }
public static async Future<int, Exception> InnerError() { yield; return Fail(); }
public static async Future<int> OuterError(bool fail) throws Exception {
yield; if (fail) throw new Exception(9); return Plain(); }
public static async Future<int, Exception> Both(bool fail) throws Exception {
yield; if (fail) throw new Exception(11); return Fail(); } }
'''

fixture.CONSUMER = '''using NativeSharp.Core;
class Program { public static async int Main() { int seen = 0;
Future<int, Exception> first = await Service.InnerError();
try { int ignored = await first; return 1; } catch (Exception e) { seen = e.Id; }
try { Future<int> second = await Service.OuterError(false); int value = await second;
if (value != 5) return 2; } catch (Exception e) { return 3; }
try { Future<int> ignored = await Service.OuterError(true); return 4; }
catch (Exception e) { if (e.Id != 9) return 5; }
try { Future<int, Exception> third = await Service.Both(false);
try { int ignored = await third; return 6; } catch (Exception e) { seen = seen + e.Id; }
} catch (Exception e) { return 7; }
try { Future<int, Exception> ignored = await Service.Both(true); return 8; }
catch (Exception e) { if (e.Id != 11) return 9; }
return seen == 14 && Service.Calls == 2 && Exception.Builds == 4 &&
Exception.Drops == 4 ? 42 : 10; } }
'''

if __name__ == '__main__':
    sys.exit(fixture.main())
