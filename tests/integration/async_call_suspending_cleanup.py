"""Check exceptions from calls across an awaiting finally in async polls."""

import sys

import async_poll_call_exception as cases


if __name__ == '__main__':
    cases.CASES = {
        name: cases.CASES[name] for name in (
            'async_cleanup_suspends', 'generator_cleanup_suspends',
            'generator_direct_cleanup_suspends', 'generator_child_error_cleanup_suspends',
            'async_cleanup', 'async_cleanup_replacement', 'generator_cleanup_nested',
            'generator_ordered_cleanup', 'local_catch', 'generator_local_catch')
    }
    cases.CASES['async_call_suspending_cleanup_replacement'] = '''
public static int Replace() throws Exception { Log = Log * 10 + 2; throw new Exception(8); }
public static async int Slow() { yield; return 9; }
public static async int Work() throws Exception { yield; owned Guard guard = new Guard();
try { return Fail(); } finally { int value = await Slow(); Log = value; int ignored = Replace(); } }
public static async int Main() { int caught = 0; try { int ignored = await Work(); return 1; }
catch (Exception e) { caught = e.Id; }
return caught == 8 && Calls == 1 && Log == 92 && Guard.Drops == 1 &&
Exception.Builds == 2 && Exception.Drops == 2 && Exception.DropTrace == 78 ? 42 : 2; }'''
    cases.CASES['generator_call_suspending_cleanup_replacement'] = '''
public static int Replace() throws Exception { Log = Log * 10 + 2; throw new Exception(8); }
public static async int Slow() { yield; return 9; }
public static AsyncIterator<int, Exception> Gen() throws Exception { owned Guard guard = new Guard();
try { yield return 1; int ignored = Fail(); }
finally { int value = await Slow(); Log = value; int ignored = Replace(); } }
public static async int Main() { int caught = 0; { AsyncIterator<int, Exception> it = Gen();
try { bool first = await it.MoveNextAsync(); if (!first || it.Current != 1) return 1;
bool second = await it.MoveNextAsync(); return 2; } catch (Exception e) { caught = e.Id; } it.Drop(); }
return caught == 8 && Calls == 1 && Log == 92 && Guard.Drops == 1 &&
Exception.Builds == 2 && Exception.Drops == 2 && Exception.DropTrace == 78 ? 42 : 3; }'''
    cases.CASES['async_call_two_suspensions'] = '''
public static async int Slow() { yield; return 9; }
public static async int Work() throws Exception { yield; owned Guard guard = new Guard();
try { return Fail(); } finally { int one = await Slow(); Log = one;
int two = await Slow(); Log = Log * 10 + two; } }
public static async int Main() { int caught = 0; try { int ignored = await Work(); return 1; }
catch (Exception e) { caught = e.Id; }
return caught == 7 && Calls == 1 && Log == 99 && Guard.Drops == 1 &&
Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 2; }'''
    cases.CASES['generator_call_two_suspensions'] = '''
public static async int Slow() { yield; return 9; }
public static AsyncIterator<int, Exception> Gen() throws Exception { owned Guard guard = new Guard();
try { yield return 1; int ignored = Fail(); } finally { int one = await Slow(); Log = one;
int two = await Slow(); Log = Log * 10 + two; } }
public static async int Main() { int caught = 0; { AsyncIterator<int, Exception> it = Gen();
try { bool first = await it.MoveNextAsync(); if (!first || it.Current != 1) return 1;
bool second = await it.MoveNextAsync(); return 2; } catch (Exception e) { caught = e.Id; } it.Drop(); }
return caught == 7 && Calls == 1 && Log == 99 && Guard.Drops == 1 &&
Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 3; }'''
    sys.exit(cases.main())
