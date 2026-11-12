"""Check calls which raise through an async poll, including cleanup ordering."""

import os
from contextlib import nullcontext
from pathlib import Path
import subprocess
import sys
from isolated_a_runner import absolute_a, setup

PREFIX = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public static int Builds; public static int Drops; public static int DropTrace;
public int Id; public Exception(int id) { Id = id; Builds = Builds + 1; }
~Exception() { Drops = Drops + 1; DropTrace = DropTrace * 10 + Id; } } }
class Guard { public static int Drops; ~Guard() { Drops = Drops + 1; } }
class OrderedGuard { ~OrderedGuard() { Program.Log = Program.Log * 10 + 2; } }
class Gate { static Gate() throws Exception { throw new Exception(8); }
public static int Read() { return 1; } }
class ThrowGuard { ~ThrowGuard() { int ignored = Gate.Read(); } }
class Program { public static int Calls; public static int Log;
public static int Fail() throws Exception { Calls = Calls + 1; throw new Exception(7); }
BODY
}'''

CASES = {
    'sync_control': '''public static int Main() { int caught = 0;
try { int ignored = Fail(); return 1; } catch (Exception e) { caught = e.Id; }
return caught == 7 && Calls == 1 && Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 2; }''',
    'async_call': '''public static async int Work() throws Exception { yield; return Fail(); }
public static async int Main() { int caught = 0; try { int ignored = await Work(); return 1; }
catch (Exception e) { caught = e.Id; }
return caught == 7 && Calls == 1 && Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 2; }''',
    'async_generator_call': '''public static AsyncIterator<int, Exception> Gen() throws Exception {
yield return 1; int ignored = Fail(); }
public static async int Main() { int caught = 0; { AsyncIterator<int, Exception> it = Gen();
try { bool first = await it.MoveNextAsync(); if (!first || it.Current != 1) return 1;
bool second = await it.MoveNextAsync(); return 2; } catch (Exception e) { caught = e.Id; } it.Drop(); }
return caught == 7 && Calls == 1 && Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 3; }''',
    'async_generator_direct_throw': '''public static AsyncIterator<int, Exception> Gen() throws Exception {
yield return 1; Calls = Calls + 1; throw new Exception(7); }
public static async int Main() { int caught = 0; { AsyncIterator<int, Exception> it = Gen();
try { bool first = await it.MoveNextAsync(); if (!first || it.Current != 1) return 1;
bool second = await it.MoveNextAsync(); return 2; } catch (Exception e) { caught = e.Id; } it.Drop(); }
return caught == 7 && Calls == 1 && Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 3; }''',
    'local_catch': '''public static async int Work() throws Exception { yield;
try { int ignored = Fail(); return 1; } catch (Exception e) { return e.Id; } }
public static async int Main() { try { int value = await Work();
return value == 7 && Calls == 1 && Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 2;
} catch (Exception e) { return 3; } }''',
    'async_cleanup': '''public static async int Work() throws Exception { yield;
owned Guard guard = new Guard(); try { return Fail(); } finally { Log = Log * 10 + 1; } }
public static async int Main() { int caught = 0; try { int ignored = await Work(); return 1; }
catch (Exception e) { caught = e.Id; }
return caught == 7 && Calls == 1 && Log == 1 && Guard.Drops == 1 &&
Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 2; }''',
    'async_cleanup_replacement': '''public static int Replace() throws Exception {
Log = Log * 10 + 2; throw new Exception(8); }
public static async int Work() throws Exception { yield; owned Guard guard = new Guard();
try { return Fail(); } finally { int ignored = Replace(); } }
public static async int Main() { int caught = 0; try { int ignored = await Work(); return 1; }
catch (Exception e) { caught = e.Id; }
return caught == 8 && Calls == 1 && Log == 2 && Guard.Drops == 1 &&
Exception.Builds == 2 && Exception.Drops == 2 && Exception.DropTrace == 78 ? 42 : 2; }''',
    'async_destructor_replacement': '''public static async int Work() throws Exception {
yield; owned ThrowGuard guard = new ThrowGuard(); return Fail(); }
public static async int Main() { int caught = 0; try { int ignored = await Work(); return 1; }
catch (Exception e) { caught = e.Id; }
return caught == 8 && Calls == 1 && Exception.Builds == 2 && Exception.Drops == 2 &&
Exception.DropTrace == 78 ? 42 : 2; }''',
    'async_cleanup_suspends': '''public static async int Slow() { yield; return 9; }
public static async int Work() throws Exception { yield; owned Guard guard = new Guard();
try { return Fail(); } finally { int value = await Slow(); Log = Log * 10 + value; } }
public static async int Main() { int caught = 0; try { int ignored = await Work(); return 1; }
catch (Exception e) { caught = e.Id; }
return caught == 7 && Calls == 1 && Log == 9 && Guard.Drops == 1 &&
Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 2; }''',
    'generator_cleanup': '''public static AsyncIterator<int, Exception> Gen() throws Exception {
owned Guard guard = new Guard(); try { yield return 1; int ignored = Fail(); }
finally { Log = Log * 10 + 1; } }
public static async int Main() { int caught = 0; { AsyncIterator<int, Exception> it = Gen();
try { bool first = await it.MoveNextAsync(); if (!first || it.Current != 1) return 1;
bool second = await it.MoveNextAsync(); return 2; } catch (Exception e) { caught = e.Id; } it.Drop(); }
return caught == 7 && Calls == 1 && Log == 1 && Guard.Drops == 1 &&
Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 3; }''',
    'generator_cleanup_nested': '''public static AsyncIterator<int, Exception> Gen() throws Exception {
owned Guard guard = new Guard(); try { try { yield return 1; int ignored = Fail(); }
finally { Log = Log * 10 + 1; } } finally { Log = Log * 10 + 2; } }
public static async int Main() { int caught = 0; { AsyncIterator<int, Exception> it = Gen();
try { bool first = await it.MoveNextAsync(); if (!first || it.Current != 1) return 1;
bool second = await it.MoveNextAsync(); return 2; } catch (Exception e) { caught = e.Id; } it.Drop(); }
return caught == 7 && Calls == 1 && Log == 12 && Guard.Drops == 1 &&
Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 3; }''',
    'generator_ordered_cleanup': '''public static AsyncIterator<int, Exception> Gen() throws Exception {
try { owned OrderedGuard guard = new OrderedGuard();
try { yield return 1; int ignored = Fail(); } finally { Log = Log * 10 + 1; } }
finally { Log = Log * 10 + 3; } }
public static async int Main() { int caught = 0; { AsyncIterator<int, Exception> it = Gen();
try { bool first = await it.MoveNextAsync(); if (!first || it.Current != 1) return 1;
bool second = await it.MoveNextAsync(); return 2; } catch (Exception e) { caught = e.Id; } it.Drop(); }
return caught == 7 && Calls == 1 && Log == 123 && Exception.Builds == 1 &&
Exception.Drops == 1 && Exception.DropTrace == 7 ? 42 : 3; }''',
    'generator_cleanup_suspends': '''public static async int Slow() { yield; return 9; }
public static AsyncIterator<int, Exception> Gen() throws Exception { owned Guard guard = new Guard();
try { yield return 1; int ignored = Fail(); } finally { int value = await Slow(); Log = value; } }
public static async int Main() { int caught = 0; { AsyncIterator<int, Exception> it = Gen();
try { bool first = await it.MoveNextAsync(); if (!first || it.Current != 1) return 1;
bool second = await it.MoveNextAsync(); return 2; } catch (Exception e) { caught = e.Id; } it.Drop(); }
return caught == 7 && Calls == 1 && Log == 9 && Guard.Drops == 1 &&
Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 3; }''',
    'generator_direct_cleanup_suspends': '''public static async int Slow() { yield; return 9; }
public static AsyncIterator<int, Exception> Gen() throws Exception { owned Guard guard = new Guard();
try { yield return 1; Calls = Calls + 1; throw new Exception(7); }
finally { int value = await Slow(); Log = value; } }
public static async int Main() { int caught = 0; { AsyncIterator<int, Exception> it = Gen();
try { bool first = await it.MoveNextAsync(); if (!first || it.Current != 1) return 1;
bool second = await it.MoveNextAsync(); return 2; } catch (Exception e) { caught = e.Id; } it.Drop(); }
return caught == 7 && Calls == 1 && Log == 9 && Guard.Drops == 1 &&
Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 3; }''',
    'generator_child_error_cleanup_suspends': '''public static async int Slow() { yield; return 9; }
public static async int Child() throws Exception { yield; Calls = Calls + 1; throw new Exception(7); }
public static AsyncIterator<int, Exception> Gen() throws Exception { owned Guard guard = new Guard();
try { yield return 1; int ignored = await Child(); }
finally { int value = await Slow(); Log = value; } }
public static async int Main() { int caught = 0; { AsyncIterator<int, Exception> it = Gen();
try { bool first = await it.MoveNextAsync(); if (!first || it.Current != 1) return 1;
bool second = await it.MoveNextAsync(); return 2; } catch (Exception e) { caught = e.Id; } it.Drop(); }
return caught == 7 && Calls == 1 && Log == 9 && Guard.Drops == 1 &&
Exception.Builds == 1 && Exception.Drops == 1 ? 42 : 3; }''',
}


def main():
    if os.name != 'nt' or len(sys.argv) != 3:
        return 77
    compiler = absolute_a(sys.argv[1])
    evidence = absolute_a(sys.argv[2])
    repository = Path(__file__).resolve().parents[2]
    environment = setup(evidence, repository)
    failures = []
    with nullcontext(evidence / 'cases') as root:
        root.mkdir()
        for name, body in CASES.items():
            case = root / name
            case.mkdir()
            (case / 'Main.ns').write_text(PREFIX.replace('BODY', body), encoding='utf-8')
            project = case / 'Probe.nsproj'
            project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                               '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = case / (level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                print('COMMAND', subprocess.list2cmdline(command), 'CWD', repository, flush=True)
                result = subprocess.run(command, cwd=repository, env=environment, capture_output=True, text=True,
                                        errors='replace', timeout=120)
                if result.returncode:
                    failures.append((name, level, 'compile', result.returncode, result.stdout + result.stderr))
                    continue
                result = subprocess.run([str(executable)], cwd=repository, env=environment, capture_output=True, text=True,
                                        errors='replace', timeout=10)
                print('RUN', name, level, result.returncode, flush=True)
                if result.returncode != 42:
                    failures.append((name, level, 'run', result.returncode, result.stdout + result.stderr))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
