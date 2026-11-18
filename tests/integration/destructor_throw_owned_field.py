"""A throwing object destructor must still clean its owned field."""
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1]).resolve()
    original = (Path(sys.argv[2]) if len(sys.argv) > 2 else
                Path(__file__).resolve().parent / 'destructor_throw_owned_field_fixture').resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-throwing-destructor-') as temporary:
        root = Path(temporary)
        for name in ('Main.ns', 'Probe.nsproj'):
            (root / name).write_bytes((original / name).read_bytes())
        original_source = (root / 'Main.ns').read_bytes()
        scenarios = {
            'outer-throws': original_source,
            'field-throws': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int Log; }
class Resource { ~Resource() { Trace.Log = Trace.Log * 10 + 1; } }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
class Bomb { ~Bomb() { Trace.Log = Trace.Log * 10 + 2; int ignored = Gate.Read(); } }
class Payload { private owned Resource first; private owned Bomb second;
 public Payload() { first = new Resource(); second = new Bomb(); }
 ~Payload() { Trace.Log = Trace.Log * 10 + 3; } }
class Program { public static int Main() { int caught = 0;
 try { Payload value = new Payload(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 10 : Trace.Log == 321 ? 42 : Trace.Log; } }
''',
            'normal': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int Log; }
class Resource { ~Resource() { Trace.Log = Trace.Log * 10 + 1; } }
class Payload { private owned Resource first; public Payload() { first = new Resource(); }
 ~Payload() { Trace.Log = Trace.Log * 10 + 3; } }
class Program { public static int Main() { { Payload value = new Payload(); }
 return Trace.Log == 31 ? 42 : 5; } }
''',
            'derived-throws': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int Log; }
class Resource { private int id; public Resource(int value) { id = value; }
 ~Resource() { Trace.Log = Trace.Log * 10 + id; } }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
class Base { private owned Resource first; public Base() { first = new Resource(1); }
 ~Base() { Trace.Log = Trace.Log * 10 + 2; } }
class Derived : Base { private owned Resource second; public Derived() { second = new Resource(3); }
 ~Derived() { Trace.Log = Trace.Log * 10 + 4; int ignored = Gate.Read(); } }
class Program { public static int Main() { int caught = 0;
 try { Derived value = new Derived(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Trace.Log == 4321 ? 42 : 6; } }
''',
            'value-field': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int Log; }
struct Part { public int Id; public Part(int id) { Id = id; }
 ~Part() { Trace.Log = Trace.Log * 10 + Id; } }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
class Payload { public Part value = new Part(1);
 ~Payload() { Trace.Log = Trace.Log * 10 + 3; int ignored = Gate.Read(); } }
class Program { public static int Main() { int caught = 0;
 try { Payload value = new Payload(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Trace.Log == 31 ? 42 : 7; } }
''',
            'fixed-array-element-throws': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int Log; }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
struct Part { public int Id; public Part(int id) { Id = id; }
 ~Part() { if (Id != 0) { Trace.Log = Trace.Log * 10 + Id; if (Id == 2) { int ignored = Gate.Read(); } } } }
class Payload { public Part[2] values; public Payload() { values[0] = new Part(1); values[1] = new Part(2); } }
class Program { public static int Main() { int caught = 0;
 try { Payload value = new Payload(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 90 + caught : Trace.Log == 21 ? 42 : Trace.Log; } }
''',
            'fixed-array-outer-cleanup': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int Log; }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
struct Part { public int Id; public Part(int id) { Id = id; }
 ~Part() { if (Id != 0) { Trace.Log = Trace.Log * 10 + Id; if (Id == 2) { int ignored = Gate.Read(); } } } }
class Resource { ~Resource() { Trace.Log = Trace.Log * 10 + 3; } }
class Payload { private owned Resource resource; public Part[2] values;
 public Payload() { resource = new Resource(); values[0] = new Part(1); values[1] = new Part(2); } }
class Program { public static int Main() { int caught = 0;
 try { Payload value = new Payload(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 90 + caught : Trace.Log == 213 ? 42 : Trace.Log; } }
''',
            'fixed-array-double-throws': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Gate1 { static Gate1() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
class Gate2 { static Gate2() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
struct Part { public int Id; public Part(int id) { Id = id; }
 ~Part() { if (Id == 2) { int ignored = Gate1.Read(); } if (Id == 1) { int ignored = Gate2.Read(); } } }
class Payload { public Part[2] values; public Payload() { values[0] = new Part(1); values[1] = new Part(2); } }
class Program { public static int Main() { int caught = 0;
 try { Payload value = new Payload(); } catch (Exception error) { caught = 1; } return 20 + caught; } }
''',
            'fixed-array-struct-outer': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int Log; }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
struct Part { public int Id; public Part(int id) { Id = id; }
 ~Part() { if (Id != 0) { Trace.Log = Trace.Log * 10 + Id; if (Id == 2) { int ignored = Gate.Read(); } } } }
class Resource { ~Resource() { Trace.Log = Trace.Log * 10 + 3; } }
struct Payload { private owned Resource resource; public Part[2] values;
 public Payload() { resource = new Resource(); values[0] = new Part(1); values[1] = new Part(2); } }
class Program { public static int Main() { int caught = 0;
 try { Payload value = new Payload(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 90 + caught : Trace.Log == 213 ? 42 : Trace.Log; } }
''',
            'dynamic-array-element-throws': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int Log; }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
struct Part { public int Id; public Part(int id) { Id = id; }
 ~Part() { if (Id != 0) { Trace.Log = Trace.Log * 10 + Id; if (Id == 2) { int ignored = Gate.Read(); } } } }
class Payload { private owned Part[] values;
 public Payload() { values = new Part[2]; values[0] = new Part(1); values[1] = new Part(2); } }
class Program { public static int Main() { int caught = 0;
 try { Payload value = new Payload(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 90 + caught : Trace.Log == 21 ? 42 : Trace.Log; } }
''',
            'dynamic-array-outer-cleanup': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int Log; }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
struct Part { public int Id; public Part(int id) { Id = id; }
 ~Part() { if (Id != 0) { Trace.Log = Trace.Log * 10 + Id; if (Id == 2) { int ignored = Gate.Read(); } } } }
class Resource { ~Resource() { Trace.Log = Trace.Log * 10 + 3; } }
class Payload { private owned Resource resource; private owned Part[] values;
 public Payload() { resource = new Resource(); values = new Part[3]; values[0] = new Part(1); values[2] = new Part(2); } }
class Program { public static int Main() { int caught = 0;
 try { Payload value = new Payload(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 90 + caught : Trace.Log == 213 ? 42 : Trace.Log; } }
''',
            'dynamic-array-double-throws': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Gate1 { static Gate1() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
class Gate2 { static Gate2() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
struct Part { public int Id; public Part(int id) { Id = id; }
 ~Part() { if (Id == 2) { int ignored = Gate1.Read(); } if (Id == 1) { int ignored = Gate2.Read(); } } }
class Payload { private owned Part[] values;
 public Payload() { values = new Part[2]; values[0] = new Part(1); values[1] = new Part(2); } }
class Program { public static int Main() { int caught = 0;
 try { Payload value = new Payload(); } catch (Exception error) { caught = 1; } return 20 + caught; } }
''',
            'dynamic-reference-array-throws': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int Log; }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
class Part { public int Id; public Part(int id) { Id = id; }
 ~Part() { Trace.Log = Trace.Log * 10 + Id; if (Id == 1) { int ignored = Gate.Read(); } } }
class Payload { private owned Part[] values;
 public Payload() { values = new Part[2]; values[0] = new Part(1); values[1] = new Part(2); } }
class Program { public static int Main() { int caught = 0;
 try { Payload value = new Payload(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 90 + caught : Trace.Log == 12 ? 42 : Trace.Log; } }
''',
            'dynamic-reference-array-outer': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int Log; }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
class Part { public int Id; public Part(int id) { Id = id; }
 ~Part() { Trace.Log = Trace.Log * 10 + Id; if (Id == 1) { int ignored = Gate.Read(); } } }
class Resource { ~Resource() { Trace.Log = Trace.Log * 10 + 3; } }
class Payload { private owned Resource resource; private owned Part[] values;
 public Payload() { resource = new Resource(); values = new Part[3]; values[0] = new Part(1); values[2] = new Part(2); } }
class Program { public static int Main() { int caught = 0;
 try { Payload value = new Payload(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 90 + caught : Trace.Log == 123 ? 42 : Trace.Log; } }
''',
            'dynamic-reference-array-double': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Gate1 { static Gate1() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
class Gate2 { static Gate2() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
class Part { public int Id; public Part(int id) { Id = id; }
 ~Part() { if (Id == 1) { int ignored = Gate1.Read(); } if (Id == 2) { int ignored = Gate2.Read(); } } }
class Payload { private owned Part[] values;
 public Payload() { values = new Part[2]; values[0] = new Part(1); values[1] = new Part(2); } }
class Program { public static int Main() { int caught = 0;
 try { Payload value = new Payload(); } catch (Exception error) { caught = 1; } return 20 + caught; } }
''',
            'dynamic-array-active-cleanup': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int Log; }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
struct Part { public int Id; public Part(int id) { Id = id; }
 ~Part() { if (Id != 0) { Trace.Log = Trace.Log * 10 + Id; if (Id == 2) { int ignored = Gate.Read(); } } } }
class Resource { ~Resource() { Trace.Log = Trace.Log * 10 + 3; } }
class Program { public static int Main() { int caught = 0;
 try { Resource outer = new Resource(); owned Part[] values = new Part[2];
 values[0] = new Part(1); values[1] = new Part(2); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 90 + caught : Trace.Log == 213 ? 42 : Trace.Log; } }
''',
            'dynamic-reference-array-active-cleanup': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int Log; }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
class Part { public int Id; public Part(int id) { Id = id; }
 ~Part() { Trace.Log = Trace.Log * 10 + Id; if (Id == 1) { int ignored = Gate.Read(); } } }
class Resource { ~Resource() { Trace.Log = Trace.Log * 10 + 3; } }
class Program { public static int Main() { int caught = 0;
 try { Resource outer = new Resource(); owned Part[] values = new Part[2];
 values[0] = new Part(1); values[1] = new Part(2); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 90 + caught : Trace.Log == 123 ? 42 : Trace.Log; } }
''',
            'dynamic-array-active-double': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Gate1 { static Gate1() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
class Gate2 { static Gate2() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
struct Part { public int Id; public Part(int id) { Id = id; }
 ~Part() { if (Id == 1) { int ignored = Gate1.Read(); } if (Id == 2) { int ignored = Gate2.Read(); } } }
class Resource { ~Resource() { } }
class Program { public static int Main() {
 try { Resource outer = new Resource(); owned Part[] values = new Part[2];
 values[0] = new Part(1); values[1] = new Part(2); } catch (Exception error) { return 42; }
 return 90; } }
''',
            'dynamic-reference-array-active-double': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Gate1 { static Gate1() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
class Gate2 { static Gate2() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
class Part { public int Id; public Part(int id) { Id = id; }
 ~Part() { if (Id == 1) { int ignored = Gate1.Read(); } if (Id == 2) { int ignored = Gate2.Read(); } } }
class Resource { ~Resource() { } }
class Program { public static int Main() {
 try { Resource outer = new Resource(); owned Part[] values = new Part[2];
 values[0] = new Part(1); values[1] = new Part(2); } catch (Exception error) { return 42; }
 return 90; } }
''',
            'unwind-struct-double': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int Drops; }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
struct Part { public int Id; public Part(int id) { Id = id; }
 ~Part() { Trace.Drops = Trace.Drops + 1; int ignored = Gate.Read(); } }
class Program { public static int Main() {
 try { Part value = new Part(1); throw new Exception(); }
 catch (Exception error) { return 50 + Trace.Drops; } } }
''',
            'unwind-struct-single': b'''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Trace { public static int Drops; }
struct Part { public int Id; public Part(int id) { Id = id; }
 ~Part() { Trace.Drops = Trace.Drops + 1; } }
class Program { public static int Main() { int caught = 0;
 try { Part value = new Part(1); throw new Exception(); }
 catch (Exception error) { caught = 1; }
 return caught == 1 && Trace.Drops == 1 ? 42 : 50 + Trace.Drops; } }
''',
        }
        scenarios['double-throws'] = (original / 'Double.ns').read_bytes()
        for scenario, source in scenarios.items():
            (root / 'Main.ns').write_bytes(source)
            debug_cases = ('normal', 'double-throws', 'dynamic-array-outer-cleanup',
                           'dynamic-reference-array-outer', 'dynamic-reference-array-double')
            levels = ('O0', 'O2', 'O0-debug') if scenario in debug_cases else ('O0', 'O2')
            for level in levels:
                executable = root / (scenario + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(root / 'Probe.nsproj'),
                           '-' + ('O0' if level == 'O0-debug' else level), '-o', str(executable), '--no-artifact-cache']
                if level == 'O0-debug':
                    command.append('-g')
                result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                        errors='replace', timeout=120)
                print(scenario, 'COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                      result.stdout + result.stderr, flush=True)
                if result.returncode:
                    failures.append((scenario, level, 'compile', result.returncode))
                    continue
                result = subprocess.run([str(executable)], cwd=root, capture_output=True, text=True,
                                        errors='replace', timeout=120)
                print(scenario, 'RUN', executable, 'EXIT', result.returncode,
                      result.stdout + result.stderr, flush=True)
                expected = 148 if scenario in ('double-throws', 'fixed-array-double-throws',
                                                'dynamic-array-double-throws',
                                                'dynamic-reference-array-double',
                                                'dynamic-array-active-double',
                                                'dynamic-reference-array-active-double',
                                                'unwind-struct-double') else 42
                if result.returncode != expected:
                    failures.append((scenario, level, 'execute', result.returncode))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('destructor_throw_owned_field: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
