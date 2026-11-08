import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(value) for value in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler, storage_source, control_source = map(Path, sys.argv[1:4])
    types = '''using NativeSharp.Core.Internal;
namespace NativeSharp.Core { public class Exception { } }
class Resource { public static int Trace; public int Id;
 public Resource(int id) { Id = id; } ~Resource() { Trace = Trace * 10 + Id; } }
class Bad { public static int Drops; public owned Resource first; public owned Resource second;
 public Bad() throws NativeSharp.Core.Exception { first = new Resource(1); second = new Resource(2); throw new NativeSharp.Core.Exception(); }
 ~Bad() { Drops = Drops + 1; } }
struct BadValue { public static int Drops; public owned Resource first; public owned Resource second;
 public BadValue() throws NativeSharp.Core.Exception { first = new Resource(3); second = new Resource(4); throw new NativeSharp.Core.Exception(); }
 ~BadValue() { Drops = Drops + 1; } }
class Early { public static int Drops; public owned Resource first;
 public Early() throws NativeSharp.Core.Exception { throw new NativeSharp.Core.Exception(); }
 ~Early() { Drops = Drops + 1; } }
class Good { public static int Drops; public long Data; public Good() { Data = 20l; } ~Good() { Drops = Drops + 1; } }
struct GoodValue { public static int Drops; public long Data; public GoodValue() { Data = 22l; } ~GoodValue() { Drops = Drops + 1; } }
'''
    valid = '''class Program { public static int Main() { unsafe("shared factory") {
 int caught = 0;
 try { SharedClassCreation<Bad> bad = SharedControlStorage.CreateThrowing<Bad>(); }
 catch (NativeSharp.Core.Exception problem) { caught = caught + 1; }
 if (Resource.Trace != 21 || Bad.Drops != 0) return 1;
 try { SharedValueCreation<BadValue> bad = SharedControlStorage.CreateValueThrowing<BadValue>(); }
 catch (NativeSharp.Core.Exception problem) { caught = caught + 1; }
 if (Resource.Trace != 2143 || BadValue.Drops != 0) return 2;
 try { SharedClassCreation<Early> bad = SharedControlStorage.CreateThrowing<Early>(); }
 catch (NativeSharp.Core.Exception problem) { caught = caught + 1; }
 if (caught != 3 || Early.Drops != 0 || Resource.Trace != 2143) return 3;
 Resource.Trace = 0; long* words = stackalloc long[32]; Bad* slot = (Bad*)words;
 try { Bad direct = new (slot) Bad(); }
 catch (NativeSharp.Core.Exception problem) { caught = caught + 1; }
 if (caught != 4 || Resource.Trace != 21 || Bad.Drops != 0) return 8;
 words[0] = 99l; if (words[0] != 99l) return 9;
 try {
  SharedClassCreation<Good> a = SharedControlStorage.CreateThrowing<Good>();
  SharedValueCreation<GoodValue> b = SharedControlStorage.CreateValueThrowing<GoodValue>();
  if (a.Block == null || a.Value == null || b.Block == null || b.Value == null) return 4;
  if (a.Value.Data + (*b.Value).Data != 42l) return 5;
  SharedStorage.DestroyObject<Good>(a.Value); destroy b.Value; delete[] a.Block; delete[] b.Block;
 } catch (NativeSharp.Core.Exception problem) { return 6; }
 return Good.Drops == 1 && GoodValue.Drops == 1 ? 42 : 7;
} } }
'''
    unsupported = '''struct Field { ~Field() { } }
class Complex { public Field field;
 public Complex() throws NativeSharp.Core.Exception { throw new NativeSharp.Core.Exception(); } }
class Program { public static int Main() {
 try { SharedClassCreation<Complex> value = SharedControlStorage.CreateThrowing<Complex>(); }
 catch (NativeSharp.Core.Exception problem) { } return 42;
} }
'''
    unsupported_base = '''class Base { }
class Derived : Base {
 public Derived() throws NativeSharp.Core.Exception { throw new NativeSharp.Core.Exception(); } }
class Program { public static int Main() {
 try { SharedClassCreation<Derived> value = SharedControlStorage.CreateThrowing<Derived>(); }
 catch (NativeSharp.Core.Exception problem) { } return 42;
} }
'''
    with tempfile.TemporaryDirectory(prefix='nsc-shared-throws-') as temporary:
        root = Path(temporary)
        for path in (storage_source, control_source):
            shutil.copyfile(path, root / path.name)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        source = root / 'Main.ns'
        source.write_text(types + valid, encoding='utf-8')
        result = run([compiler, '--dump-project-oop-llvm', project, '--no-banner'], root)
        assert result.returncode == 0, result.stdout + result.stderr
        body = re.search(r'define[^\n]*SharedControlStorage__CreateThrowing__g1__Bad__[^\n]*\{(.*?)\n\}',
                         result.stdout, re.DOTALL)
        assert body and 'cleanuppad' in body.group(1) and '@__nativesharp_free' in body.group(1), 'missing block cleanup in factory IR'
        assert 'placement.fields.failure' in result.stdout, 'missing owned-field cleanup in placement IR'
        if os.name == 'nt':
            for level in ('O0', 'O2'):
                exe = root / (level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-o', exe,
                              '--no-artifact-cache', '-' + level], root)
                assert result.returncode == 0, result.stdout + result.stderr
                result = run([exe], root)
                assert result.returncode == 42, (level, result.returncode, result.stdout, result.stderr)
        for negative in (unsupported, unsupported_base):
            source.write_text(types + negative, encoding='utf-8')
            result = run([compiler, '--dump-project-oop-llvm', project, '--no-banner'], root)
            assert result.returncode != 0 and 'shared throwing construction cannot safely clean this payload shape' in result.stdout + result.stderr
    return 0


if __name__ == '__main__':
    sys.exit(main())
