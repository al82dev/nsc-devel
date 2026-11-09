from pathlib import Path
import os
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1])
    cases = {
        'explicit': '''namespace NativeSharp.Core { public class Exception { } }
class Base { public static int Built; public static int Drops;
 public Base() { Built = Built + 1; } ~Base() { Drops = Drops + 1; } }
class Derived : Base { public static int Drops;
 public Derived() throws NativeSharp.Core.Exception { throw new NativeSharp.Core.Exception(); }
 ~Derived() { Drops = Drops + 1; } }
class Program { public static int Main() { try { Derived value = new Derived(); }
 catch (NativeSharp.Core.Exception problem) { }
 return Base.Built == 1 && Base.Drops == 1 && Derived.Drops == 0 ? 42 : 1;
} }''',
        'implicit': '''namespace NativeSharp.Core { public class Exception { } }
class Resource { public static int Trace; private int id;
 public Resource(int value) { id = value; } ~Resource() { Trace = Trace * 10 + id; } }
class Base { public owned Resource baseField; public Base() { baseField = new Resource(1); }
 ~Base() { Resource.Trace = Resource.Trace * 10 + 2; } }
class Middle : Base { public owned Resource middleField = new Resource(4);
 ~Middle() { Resource.Trace = Resource.Trace * 10 + 3; } }
class Derived : Middle { public owned Resource derivedField;
 public Derived() throws NativeSharp.Core.Exception {
  derivedField = new Resource(5); throw new NativeSharp.Core.Exception(); }
 ~Derived() { Resource.Trace = Resource.Trace * 10 + 6; } }
class Program { public static int Main() {
 try { Derived value = new Derived(); } catch (NativeSharp.Core.Exception error) { }
 return Resource.Trace == 53421 ? 42 : 1;
} }''',
        'helper_owned': '''namespace NativeSharp.Core { public class Exception { } }
class Resource { public static int Drops; ~Resource() { Drops = Drops + 1; } }
class Base { public static int Drops; ~Base() { Drops = Drops + 1; } }
class Derived : Base { public owned Resource field;
 public void Initialize() { field = new Resource(); }
 public Derived() throws NativeSharp.Core.Exception {
  Initialize(); throw new NativeSharp.Core.Exception(); }
}
class Program { public static int Main() {
 try { Derived value = new Derived(); } catch (NativeSharp.Core.Exception error) { }
 return Base.Drops == 1 && Resource.Drops == 1 ? 42 : 10 + Base.Drops * 10 + Resource.Drops;
} }''',
    }
    with tempfile.TemporaryDirectory(prefix='nsc-constructor-throwing-base-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                           '</ItemGroup></Project>', encoding='utf-8')
        failures = []
        for name, source in cases.items():
            (root / 'Main.ns').write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-o',
                           str(executable), '--no-artifact-cache', '-' + level]
                result = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                assert result.returncode == 0, (name, level, result.stdout + result.stderr)
                if os.name == 'nt':
                    result = subprocess.run([str(executable)], cwd=root, capture_output=True, timeout=120)
                    print(name, level, 'exit=' + str(result.returncode), flush=True)
                    if result.returncode != 42:
                        failures.append((name, level, result.returncode))
        assert not failures, failures
    return 0


if __name__ == '__main__':
    sys.exit(main())
