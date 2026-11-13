"""Exercise child-future errors while the parent finally suspends."""

import sys

import async_poll_call_exception as cases


if __name__ == '__main__':
    cases.CASES = {
        'generator_child_error_cleanup_suspends': cases.CASES['generator_child_error_cleanup_suspends'],
        'generator_local_catch': cases.CASES['generator_local_catch'],
        'local_catch': cases.CASES['local_catch'],
        'async_child_error_cleanup_suspends': '''public static async int Slow() { yield; return 9; }
public static async int Child() throws Exception { yield; Calls = Calls + 1; throw new Exception(7); }
public static async int Work() throws Exception { yield; owned Guard guard = new Guard();
try { int ignored = await Child(); return ignored; }
finally { int value = await Slow(); Log = value; } }
public static async int Main() { int caught = 0; try { int ignored = await Work(); return 1; }
catch (Exception e) { caught = e.Id; }
return caught == 7 && Calls == 1 && Log == 9 && Guard.Drops == 1 &&
Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 2; }''',
    }
    sys.exit(cases.main())
