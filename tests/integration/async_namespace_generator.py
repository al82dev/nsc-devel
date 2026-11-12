"""Async generators declared at namespace scope retain their own coroutine machinery."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
from xml.sax.saxutils import quoteattr


CASES = {
    'single': '''namespace Helpers { public AsyncIterator<int, int> Gen() { yield return 42; } }
class Program { public static int Main() { AsyncIterator<int, int> it = Helpers.Gen();
bool first = await it.MoveNextAsync(); return first ? it.Current : 1; } }''',
    'scalar_parameter': '''namespace Helpers { public AsyncIterator<int, int> Gen(int value) {
yield return value; yield return value; } }
class Program { public static async int Main() { AsyncIterator<int, int> it = Helpers.Gen(21);
bool first = await it.MoveNextAsync(); int a = it.Current;
bool second = await it.MoveNextAsync(); int b = it.Current;
bool last = await it.MoveNextAsync(); return first && second && !last && a+b == 42 ? 42 : 1; } }''',
    'struct_parameter_await': '''struct Box { public int Value; public Box(int value) { Value = value; }
public int Get() { return Value; } }
class Clock { public static async int Slow() { yield; return 1; } }
namespace Helpers { public AsyncIterator<int, int> Gen(Box box) {
yield return box.Get(); int ignored = await Clock.Slow(); yield return box.Get(); } }
class Program { public static async int Main() { AsyncIterator<int, int> it = Helpers.Gen(new Box(21));
bool first = await it.MoveNextAsync(); int a = it.Current;
bool second = await it.MoveNextAsync(); int b = it.Current;
bool last = await it.MoveNextAsync(); return first && second && !last && a+b == 42 ? 42 : 1; } }''',
    'cancel_cleanup': '''class Trace { public static int Value; }
namespace Helpers { public AsyncIterator<int, int> Gen() { try { yield return 5; }
finally { Trace.Value = Trace.Value + 7; } } }
class Program { public static async int Main() { AsyncIterator<int, int> it = Helpers.Gen();
bool first = await it.MoveNextAsync(); if (!first || it.Current != 5) return 1;
it.Cancel(); if (Trace.Value != 0) return 2; bool next = await it.MoveNextAsync();
if (next || Trace.Value != 7) return 3; it.Drop(); return Trace.Value == 7 ? 42 : 4; } }''',
    'generic_specializations': '''namespace Helpers { public AsyncIterator<T, int> Gen<T>(T value) {
yield return value; } }
class Payload { public int Value; public Payload(int value) { Value = value; } }
class Program { public static async int Main() { AsyncIterator<int, int> a = Helpers.Gen<int>(21);
bool first = await a.MoveNextAsync(); int x = a.Current;
owned Payload owner = new Payload(21); AsyncIterator<Payload, int> b = Helpers.Gen<Payload>(owner);
bool second = await b.MoveNextAsync(); return first && second && x + b.Current.Value == 42 ? 42 : 1; } }''',
    'prototype_then_definition': '''namespace Helpers { public AsyncIterator<int, int> Gen(int value);
public AsyncIterator<int, int> Gen(int value) { yield return value; } }
class Program { public static int Main() { AsyncIterator<int, int> it = Helpers.Gen(42);
bool first = await it.MoveNextAsync(); return first ? it.Current : 1; } }''',
    'definition_then_prototype': '''namespace Helpers { public AsyncIterator<int, int> Gen(int value) { yield return value; }
public AsyncIterator<int, int> Gen(int value); }
class Program { public static int Main() { AsyncIterator<int, int> it = Helpers.Gen(42);
bool first = await it.MoveNextAsync(); return first ? it.Current : 1; } }''',
    'namespace_factory': '''namespace Helpers { public AsyncIterator<int, int> Gen() { return Program.Make(); } }
class Program { public static AsyncIterator<int, int> Make() { yield return 42; }
public static int Main() { AsyncIterator<int, int> it = Helpers.Gen();
bool first = await it.MoveNextAsync(); return first ? it.Current : 1; } }''',
    'ordinary_namespace': '''namespace Helpers { public int Get() { return 42; } }
class Program { public static int Main() { return Helpers.Get(); } }''',
    'unused_bodyless_namespace': '''namespace Helpers { public AsyncIterator<int, int> Gen(); }
class Program { public static int Main() { return 42; } }''',
    'sync_generator_class': '''class Program { public static Iterator<int> Gen() { yield return 42; }
public static int Main() { Iterator<int> it = Gen(); return it.MoveNext() ? it.Current : 1; } }''',
    'yield_break': '''namespace Helpers { public AsyncIterator<int, int> Gen() { yield break; } }
class Program { public static int Main() { AsyncIterator<int, int> it = Helpers.Gen();
bool next = await it.MoveNextAsync(); return next ? 1 : 42; } }''',
    'yield_break_cleanup_namespace': '''class Trace { public static int Value; }
class Guard { public static int Drops; ~Guard() { Drops = Drops + 1; } }
namespace Helpers { public AsyncIterator<int, int> Gen() { owned Guard guard = new Guard();
try { yield return 5; yield break; } finally { Trace.Value = Trace.Value + 7; } } }
class Program { public static async int Main() { AsyncIterator<int, int> it = Helpers.Gen();
bool first = await it.MoveNextAsync(); if (!first || it.Current != 5) return 1;
bool second = await it.MoveNextAsync(); if (second || Trace.Value != 7 || Guard.Drops != 1) return 2;
bool third = await it.MoveNextAsync(); it.Drop(); return !third && Trace.Value == 7 && Guard.Drops == 1 ? 42 : 3; } }''',
    'yield_break_cleanup_class': '''class Trace { public static int Value; }
class Guard { public static int Drops; ~Guard() { Drops = Drops + 1; } }
class Program { public static AsyncIterator<int, int> Gen() { owned Guard guard = new Guard();
try { yield return 5; yield break; } finally { Trace.Value = Trace.Value + 7; } }
public static async int Main() { AsyncIterator<int, int> it = Gen();
bool first = await it.MoveNextAsync(); if (!first || it.Current != 5) return 1;
bool second = await it.MoveNextAsync(); if (second || Trace.Value != 7 || Guard.Drops != 1) return 2;
bool third = await it.MoveNextAsync(); it.Drop(); return !third && Trace.Value == 7 && Guard.Drops == 1 ? 42 : 3; } }''',
    'yield_break_in_catch': '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public static int Drops; ~Exception() { Drops = Drops + 1; } } }
class Trace { public static int Value; }
namespace Helpers { public AsyncIterator<int, Exception> Gen() throws Exception {
try { throw new Exception(); } catch (Exception error) { yield break; }
finally { Trace.Value = Trace.Value + 7; } } }
class Program { public static async int Main() { try { { AsyncIterator<int, Exception> it = Helpers.Gen();
bool next = await it.MoveNextAsync(); if (next) return 1; it.Drop(); }
return Exception.Drops == 1 && Trace.Value == 7 ? 42 : 2;
} catch (Exception error) { return 3; } } }''',
    'class_yield_break_nested_catch': '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public static int Drops; ~Exception() { Drops = Drops + 1; } } }
class Trace { public static int Value; }
class Helpers { public static AsyncIterator<int, Exception> Gen() throws Exception {
try { throw new Exception(); } catch (Exception outer) {
try { throw new Exception(); } catch (Exception inner) { yield break; } }
finally { Trace.Value = Trace.Value + 7; } } }
class Program { public static async int Main() { try { { AsyncIterator<int, Exception> it = Helpers.Gen();
bool next = await it.MoveNextAsync(); if (next) return 1; it.Drop(); }
return Exception.Drops == 2 && Trace.Value == 7 ? 42 : 2;
} catch (Exception error) { return 3; } } }''',
    'class_yield_break_catch_cleanup_throws': '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public static int Drops; ~Exception() { Drops = Drops + 1; } } }
class Helpers { public static AsyncIterator<int, Exception> Gen() throws Exception {
try { throw new Exception(); } catch (Exception error) { yield break; }
finally { throw new Exception(); } } }
class Program { public static async int Main() { int caught = 0;
{ AsyncIterator<int, Exception> it = Helpers.Gen(); try { bool next = await it.MoveNextAsync(); return 1; }
catch (Exception error) { caught = 1; } it.Drop(); }
return caught == 1 && Exception.Drops == 2 ? 42 : 2; } }''',
    'class_yield_break_nested_finally': '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public static int Drops; ~Exception() { Drops = Drops + 1; } } }
class Trace { public static int Value; }
class Helpers { public static AsyncIterator<int, Exception> Gen() throws Exception {
try { throw new Exception(); } catch (Exception outer) {
try { try { throw new Exception(); } catch (Exception inner) { yield break; } }
finally { Trace.Value = Trace.Value * 10 + Exception.Drops; } }
finally { Trace.Value = Trace.Value * 10 + Exception.Drops; } } }
class Program { public static async int Main() { try { { AsyncIterator<int, Exception> it = Helpers.Gen();
bool next = await it.MoveNextAsync(); if (next) return 1; it.Drop(); }
return Exception.Drops == 2 && Trace.Value == 12 ? 42 : 2;
} catch (Exception error) { return 3; } } }''',
    'namespace_yield_break_nested_finally': '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public static int Drops; ~Exception() { Drops = Drops + 1; } } }
class Trace { public static int Value; }
namespace Helpers { public AsyncIterator<int, Exception> Gen() throws Exception {
try { throw new Exception(); } catch (Exception outer) {
try { try { throw new Exception(); } catch (Exception inner) { yield break; } }
finally { Trace.Value = Trace.Value * 10 + Exception.Drops; } }
finally { Trace.Value = Trace.Value * 10 + Exception.Drops; } } }
class Program { public static async int Main() { try { { AsyncIterator<int, Exception> it = Helpers.Gen();
bool next = await it.MoveNextAsync(); if (next) return 1; it.Drop(); }
return Exception.Drops == 2 && Trace.Value == 12 ? 42 : 2;
} catch (Exception error) { return 3; } } }''',
    'yield_break_conditional_cleanup': '''class Trace { public static int Value; }
class Guard { public static int Drops; ~Guard() { Drops = Drops + 1; } }
namespace Helpers { public AsyncIterator<int, int> Gen(bool stop) { owned Guard guard = new Guard();
try { if (stop) yield break; yield return 7; } finally { Trace.Value = Trace.Value + 1; } } }
class Program { public static async int Main() {
{ AsyncIterator<int, int> first = Helpers.Gen(true); bool next = await first.MoveNextAsync();
if (next || Trace.Value != 1 || Guard.Drops != 1) return 1; first.Drop(); }
{ AsyncIterator<int, int> second = Helpers.Gen(false); bool next = await second.MoveNextAsync();
if (!next || second.Current != 7 || Trace.Value != 1 || Guard.Drops != 1) return 2;
next = await second.MoveNextAsync(); if (next) return 3; second.Drop(); }
return Trace.Value == 2 && Guard.Drops == 2 ? 42 : 4; } }''',
    'yield_break_cleanup_throws': '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { public static int Drops; ~Exception() { Drops = Drops + 1; } } }
namespace Helpers { public AsyncIterator<int, Exception> Gen() throws Exception {
try { yield break; } finally { throw new Exception(); } } }
class Program { public static async int Main() { int caught = 0;
{ AsyncIterator<int, Exception> it = Helpers.Gen(); try { bool next = await it.MoveNextAsync(); return 1; }
catch (Exception error) { caught = 1; } it.Drop(); }
return caught == 1 && Exception.Drops == 1 ? 42 : 2; } }''',
    'owned_parameter': '''class Payload { public static int Drops; public int Value;
public Payload(int value) { Value = value; } ~Payload() { Drops = Drops + 1; } }
namespace Helpers { public AsyncIterator<int, int> Gen(owned Payload value) { yield return value.Value; } }
class Program { public static async int Main() { owned Payload owner = new Payload(42);
{ AsyncIterator<int, int> it = Helpers.Gen(move(owner)); bool next = await it.MoveNextAsync();
if (!next || it.Current != 42) return 1; it.Drop(); } return Payload.Drops == 1 ? 42 : 2; } }''',
    'ref_parameter': '''namespace Helpers { public AsyncIterator<int, int> Gen(ref int value) { yield return value; } }
class Program { public static int Main() { int value = 42; AsyncIterator<int, int> it = Helpers.Gen(ref value);
bool next = await it.MoveNextAsync(); return next ? it.Current : 1; } }''',
    'in_parameter': '''namespace Helpers { public AsyncIterator<int, int> Gen(in int value) { yield return value; } }
class Program { public static int Main() { int value = 42; AsyncIterator<int, int> it = Helpers.Gen(in value);
bool next = await it.MoveNextAsync(); return next ? it.Current : 1; } }''',
    'factory_nested_lambda': '''delegate int Read();
namespace Helpers { public AsyncIterator<int, int> Gen() { raii Read read = () => 1;
int observed = read(); return Program.Make(); } }
class Program { public static AsyncIterator<int, int> Make() { yield return 42; }
public static int Main() { AsyncIterator<int, int> it = Helpers.Gen();
bool first = await it.MoveNextAsync(); return first ? it.Current : 1; } }''',
}


def check_binary(compiler, repository, root, failures):
    producer = root / 'producer'
    consumer = root / 'consumer'
    producer.mkdir()
    consumer.mkdir()
    project = producer / 'Producer.nsproj'
    project.write_text('<Project><PropertyGroup><Name>Producer</Name><OutputType>Library</OutputType>'
                       '</PropertyGroup><ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
    (producer / 'Main.ns').write_text('namespace Helpers { public AsyncIterator<int, int> Gen(int value) '
                                     '{ yield return value; } }', encoding='utf-8')
    (consumer / 'Main.ns').write_text('class Program { public static int Main() { '
        'AsyncIterator<int, int> it = Helpers.Gen(42); bool next = await it.MoveNextAsync(); '
        'return next ? it.Current : 1; } }', encoding='utf-8')
    for level in ('O0', 'O2'):
        library = producer / ('Producer-' + level + '.lib')
        command = [str(compiler), '--emit-project-oop-lib', str(project), '-' + level,
                   '-o', str(library), '--no-artifact-cache']
        print('COMMAND', subprocess.list2cmdline(command), 'CWD', repository, flush=True)
        result = subprocess.run(command, cwd=repository, capture_output=True, text=True, errors='replace', timeout=120)
        if result.returncode:
            failures.append(('binary', level, 'producer', result.returncode, result.stdout + result.stderr))
            continue
        metadata = library.with_suffix('.nsmeta')
        if not metadata.is_file():
            failures.append(('binary', level, 'missing-metadata'))
            continue
        consumer_project = consumer / 'Consumer.nsproj'
        consumer_project.write_text('<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup>'
            '<ItemGroup><Compile Include="Main.ns" /><LibraryReference Include=' + quoteattr(library.as_posix()) +
            ' Metadata=' + quoteattr(metadata.as_posix()) + ' /></ItemGroup></Project>', encoding='utf-8')
        executable = consumer / ('Consumer-' + level + '.exe')
        command = [str(compiler), '--emit-project-oop-exe', str(consumer_project), '-' + level,
                   '-o', str(executable), '--no-artifact-cache']
        print('COMMAND', subprocess.list2cmdline(command), 'CWD', repository, flush=True)
        result = subprocess.run(command, cwd=repository, capture_output=True, text=True, errors='replace', timeout=120)
        if result.returncode:
            failures.append(('binary', level, 'consumer', result.returncode, result.stdout + result.stderr))
            continue
        try:
            result = subprocess.run([str(executable)], cwd=repository, capture_output=True,
                                    text=True, errors='replace', timeout=10)
        except subprocess.TimeoutExpired:
            failures.append(('binary', level, 'timeout'))
            continue
        print('binary', level, 'exit=' + str(result.returncode), flush=True)
        if result.returncode != 42:
            failures.append(('binary', level, 'run', result.returncode, result.stdout + result.stderr))


def main():
    if os.name != 'nt':
        return 77
    compiler = Path(sys.argv[1]).resolve()
    repository = Path(__file__).resolve().parents[2]
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-namespace-async-generator-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        for name, source in CASES.items():
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                print('COMMAND', subprocess.list2cmdline(command), 'CWD', repository, flush=True)
                build = subprocess.run(command, cwd=repository, capture_output=True, text=True,
                                       errors='replace', timeout=120)
                if build.returncode:
                    failures.append((name, level, 'compile', build.returncode, build.stdout + build.stderr))
                    continue
                try:
                    run = subprocess.run([str(executable)], cwd=repository, capture_output=True,
                                         text=True, errors='replace', timeout=10)
                except subprocess.TimeoutExpired:
                    failures.append((name, level, 'timeout'))
                    continue
                print(name, level, 'exit=' + str(run.returncode), flush=True)
                if run.returncode != 42:
                    failures.append((name, level, 'run', run.returncode, run.stdout + run.stderr))
        check_binary(compiler, repository, root, failures)
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
