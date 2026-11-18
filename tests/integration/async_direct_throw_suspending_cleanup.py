"""Check that direct throws publish their error only after an awaiting finally."""

import sys

import async_poll_call_exception as cases


if __name__ == '__main__':
    cases.CASES = {
        'generator_direct_cleanup_suspends': cases.CASES['generator_direct_cleanup_suspends'],
        'async_generator_direct_throw': cases.CASES['async_generator_direct_throw'],
        'async_cleanup_direct_throw_suspends': '''public static async int Slow() { yield; return 9; }
public static async int Work() throws Exception { yield; owned Guard guard = new Guard();
try { Calls = Calls + 1; throw new Exception(7); }
finally { int value = await Slow(); Log = value; } }
public static async int Main() { int caught = 0; try { int ignored = await Work(); return 1; }
catch (Exception e) { caught = e.Id; }
return caught == 7 && Calls == 1 && Log == 9 && Guard.Drops == 1 &&
Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 2; }''',
    }
    sys.exit(cases.main())
