from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(item) for item in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler = Path(sys.argv[1])
    cases = {
        'direct_rc': ('class Holder { public static NativeSharp.Core.Rc<Payload> value; }', True),
        'direct_weak': ('class Holder { public static WeakRc<Payload> value; }', True),
        'public_wrapper': ('class Holder { public static Wrapper value; }', True),
        'private_wrapper': ('class Holder { public static PrivateWrapper value; }', True),
        'generic_wrapper': ('class Holder { public static GenericWrapper<WeakRc<Payload>> value; }', True),
        'nested_wrapper': ('class Holder { public static NestedWrapper value; }', True),
        'binary_bridge': ('class Holder { public static Bridge.Outer value; }', True),
        'consumer_wrapper': ('struct LocalWrapper { public WeakRc<Payload> value; } '
                             'class Holder { public static LocalWrapper value; }', True),
        'derived': ('class Holder { public static Derived value; }', True),
        'base': ('class Holder { public static Base value; }', True),
        'array': ('class Holder { public static NativeSharp.Core.Rc<Payload>[] value; }', True),
        'fixed_array': ('class Holder { public static WeakRc<Payload>[2] value; }', True),
        'pointer': ('class Holder { public static WeakRc<Payload>* value; }', True),
        'generic_static': ('class Box<T> { public static T value; } '
                           'class Holder { public Box<WeakRc<Payload>> value; }', True),
        'atomic': ('class Holder { public static WeakArc<Payload> value; }', False),
        'unrelated': ('class Holder { public static Other.Rc<Payload> value; }', False),
    }
    with tempfile.TemporaryDirectory(prefix='nsc-shared-static-binary-') as temporary:
        root = Path(temporary)
        producer = root / 'Producer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>Producer</Name><Language>3</Language>'
                            '<OutputType>Library</OutputType></PropertyGroup><ItemGroup>'
                            '<Compile Include="Producer.ns" /></ItemGroup></Project>', encoding='utf-8')
        (root / 'Producer.ns').write_text('''namespace NativeSharp.Core {
 public struct Rc<T> { } public struct WeakRc<T> { }
 public struct Arc<T> { } public struct WeakArc<T> { }
}
namespace Other { public struct Rc<T> { } }
namespace Demo {
 public class Payload { }
 public struct Wrapper { public NativeSharp.Core.WeakRc<Payload> value; }
 public struct PrivateWrapper { private NativeSharp.Core.WeakRc<Payload> value; }
 public struct GenericWrapper<T> { public T value; }
 public struct NestedWrapper { public Wrapper value; }
 public class Base { }
 public class Derived : Base { public NativeSharp.Core.WeakRc<Payload> value; }
}''', encoding='utf-8')
        library = root / 'Producer.lib'
        command = [compiler, '--emit-project-oop-lib', producer, '-O0', '-o', library]
        result = run(command, root)
        print('producer command:', ' '.join(map(str, command)), flush=True)
        print('producer exit:', result.returncode, flush=True)
        print(result.stdout + result.stderr, flush=True)
        if result.returncode:
            return 1
        bridge = root / 'Bridge.nsproj'
        bridge.write_text('<Project><PropertyGroup><Name>Bridge</Name><Language>3</Language>'
                          '<OutputType>Library</OutputType></PropertyGroup><ItemGroup>'
                          '<Compile Include="Bridge.ns" />'
                          '<LibraryReference Include="Producer.lib" Metadata="Producer.nsmeta" />'
                          '</ItemGroup></Project>', encoding='utf-8')
        (root / 'Bridge.ns').write_text('''namespace Bridge {
 public struct Outer { private Demo.Wrapper value; }
}''', encoding='utf-8')
        bridge_library = root / 'Bridge.lib'
        command = [compiler, '--emit-project-oop-lib', bridge, '-O0', '-o', bridge_library]
        result = run(command, root)
        print('bridge command:', ' '.join(map(str, command)), flush=True)
        print('bridge exit:', result.returncode, flush=True)
        print(result.stdout + result.stderr, flush=True)
        if result.returncode:
            return 1
        consumer = root / 'Consumer.nsproj'
        consumer.write_text('<Project><PropertyGroup><Name>Consumer</Name><Language>3</Language>'
                            '</PropertyGroup><ItemGroup><Compile Include="Consumer.ns" />'
                            '<LibraryReference Include="Producer.lib" Metadata="Producer.nsmeta" />'
                            '<LibraryReference Include="Bridge.lib" Metadata="Bridge.nsmeta" />'
                            '</ItemGroup></Project>', encoding='utf-8')
        source = root / 'Consumer.ns'
        failures = 0
        for name, (declaration, rejected) in cases.items():
            source.write_text('using NativeSharp.Core; using Demo; ' + declaration +
                              ' class Program { public static int Main() { return 42; } }', encoding='utf-8')
            command = [compiler, '--check-project-method-bodies', consumer]
            result = run(command, root)
            output = result.stdout + result.stderr
            passed = (result.returncode != 0 and 'thread-local shared pointer' in output) if rejected else result.returncode == 0
            failures += not passed
            print(('PASS ' if passed else 'FAIL ') + name + ' exit=' + str(result.returncode), flush=True)
            print('command:', ' '.join(map(str, command)), flush=True)
            print(output, flush=True)
        return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
