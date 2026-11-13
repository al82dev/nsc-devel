"""Cancellation preserves exceptions raised by an awaited cleanup operation."""

import sys

import async_poll_call_exception as fixture


GENERATOR = '''public static async int Later() throws Exception {
 yield; Calls = Calls + 1; throw new Exception(7); }
public static AsyncIterator<int, Exception> Gen() throws Exception {
 try { owned Guard guard = new Guard(); yield return 1; }
 finally { Log = Log * 10 + 1; yield return 5; Log = Log * 10 + 2;
  int ignored = await Later(); yield return 6; Log = Log * 10 + 3; } }
'''

MAIN = '''public static async int Main() { int caught = 0; {
 AsyncIterator<int, Exception> it = Gen();
 try { if (!await it.MoveNextAsync() || it.Current != 1) return 20;
  STARTED
  it.Cancel(); if (await it.MoveNextAsync()) return 21; return 22;
 } catch (Exception error) { caught = error.Id; }
 it.Drop(); }
 return caught == 7 && Calls == 1 && Log == 12 && Guard.Drops == 1 &&
  Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 23; }'''

CALL_GENERATOR = '''public static AsyncIterator<int, Exception> Gen() throws Exception {
 try { owned Guard guard = new Guard(); yield return 1; int ignored = Fail(); }
 finally { Log = Log * 10 + 1; yield return 5; Log = Log * 10 + 2; } }
'''

CALL_MAIN = '''public static async int Main() { int caught = 0; {
 AsyncIterator<int, Exception> it = Gen();
 try { if (!await it.MoveNextAsync() || it.Current != 1) return 20;
  if (!await it.MoveNextAsync() || it.Current != 5) return 21;
  ACTION
 } catch (Exception error) { caught = error.Id; }
 it.Drop(); }
 return caught == EXPECTED && Calls == 1 && Log == 12 && Guard.Drops == 1 &&
  Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 23; }'''

NESTED_GENERATOR = '''public static AsyncIterator<int, Exception> Gen() throws Exception {
 try { try { owned Guard guard = new Guard(); yield return 1; int ignored = Fail(); }
  finally { Log = Log * 10 + 1; yield return 5; Log = Log * 10 + 2; } }
 finally { Log = Log * 10 + 3; } }
'''


def main():
    fixture.CASES = {
        'cancel_before_finally': GENERATOR + MAIN.replace('STARTED', ''),
        'cancel_during_finally': GENERATOR + MAIN.replace(
            'STARTED', 'if (!await it.MoveNextAsync() || it.Current != 5) return 24;'),
        'call_exception_resume': CALL_GENERATOR + CALL_MAIN.replace(
            'ACTION', 'if (await it.MoveNextAsync()) return 22; return 24;').replace('EXPECTED', '7'),
        'call_exception_cancel': CALL_GENERATOR + CALL_MAIN.replace(
            'ACTION', 'it.Cancel(); if (await it.MoveNextAsync()) return 22;').replace('EXPECTED', '0'),
        'call_exception_drop': CALL_GENERATOR + CALL_MAIN.replace(
            'ACTION', 'it.Drop();').replace('EXPECTED', '0'),
        'call_exception_nested': NESTED_GENERATOR + CALL_MAIN.replace(
            'ACTION', 'if (await it.MoveNextAsync()) return 22; return 24;')
            .replace('EXPECTED', '7').replace('Log == 12', 'Log == 123'),
        'call_exception_nested_cancel': NESTED_GENERATOR + CALL_MAIN.replace(
            'ACTION', 'it.Cancel(); if (await it.MoveNextAsync()) return 22;')
            .replace('EXPECTED', '0').replace('Log == 12', 'Log == 123'),
        'call_exception_nested_drop': NESTED_GENERATOR + CALL_MAIN.replace('ACTION', 'it.Drop();')
            .replace('EXPECTED', '0').replace('Log == 12', 'Log == 123'),
        'call_exception_replacement': '''public static int Replace() throws Exception {
 Calls = Calls + 1; throw new Exception(8); }
public static AsyncIterator<int, Exception> Gen() throws Exception {
 try { owned Guard guard = new Guard(); yield return 1; int ignored = Fail(); }
 finally { Log = 1; yield return 5; int replaced = Replace(); } }
''' + CALL_MAIN.replace('ACTION', 'if (await it.MoveNextAsync()) return 22; return 24;')
            .replace('EXPECTED', '8').replace('Calls == 1', 'Calls == 2').replace('Log == 12', 'Log == 1')
            .replace('Exception.Builds == 1 && Exception.Drops == 1',
                     'Exception.Builds == 2 && Exception.Drops == 2 && Exception.DropTrace == 78'),
    }
    return fixture.main()


if __name__ == '__main__':
    sys.exit(main())
