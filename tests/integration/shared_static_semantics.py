from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1])
    prefix = '''namespace NativeSharp.Core {
 public struct Rc<T> { public int data; } public struct WeakRc<T> { public int data; }
 public struct Arc<T> { public T* data; } public struct WeakArc<T> { public T* data; }
} using NativeSharp.Core; struct Payload { public int data; }
'''
    cases = {
        'strong': ('class Box { public static Rc<Payload> value; }', '', True),
        'weak': ('class Box { public static WeakRc<Payload> value; }', '', True),
        'nested': ('struct Inner { public WeakRc<Payload> value; } struct Outer { public Inner value; } '
                   'class Box { public static Outer value; }', '', True),
        'generic': ('struct Generic<T> { public T value; } '
                    'class Box { public static Generic<WeakRc<Payload>> value; }', '', True),
        'generic_static_field': ('class Generic<T> { public static T value; }',
                                 'Generic<WeakRc<Payload>> box;', True),
        'array': ('class Box { public static Rc<Payload>[] value; }', '', True),
        'fixed_array': ('class Box { public static Rc<Payload>[2] value; }', '', True),
        'pointer': ('class Box { public static WeakRc<Payload>* value; }', '', True),
        'atomic_nested': ('class Box { public static Arc<WeakRc<Payload>> value; }', '', True),
        'static_local': ('', 'static WeakRc<Payload> value;', True),
        'namespace': ('namespace Global { public WeakRc<Payload> value; }', '', True),
        'inherited': ('class Base { public WeakRc<Payload> value; } class Derived : Base { } '
                      'class Box { public static Derived value; }', '', True),
        'polymorphic': ('class Base { } class Derived : Base { public WeakRc<Payload> value; } '
                        'class Box { public static Base value; }', '', True),
        'interface': ('interface IBase { } class Derived : IBase { public WeakRc<Payload> value; } '
                      'class Box { public static IBase value; }', '', True),
        'safe_sibling': ('class Base { } class Dangerous : Base { public WeakRc<Payload> value; } '
                         'class Safe : Base { } class Box { public static Safe value; }', '', False),
        'ordinary_cycle': ('class Node { public Node next; } class Box { public static Node value; }', '', False),
        'atomic': ('class Box { public static WeakArc<Payload> value; }', '', False),
        'local': ('', 'WeakRc<Payload> value;', False),
        'other_namespace': ('namespace Other { public struct Rc<T> { public int value; } } '
                            'class Box { public static Other.Rc<Payload> value; }', '', False),
    }
    with tempfile.TemporaryDirectory(prefix='nsc-shared-static-sema-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>', encoding='utf-8')
        for name, (declarations, local, rejected) in cases.items():
            (root / 'Main.ns').write_text(prefix + declarations +
                ' class Program { public static int Main() { ' + local + ' return 42; } }', encoding='utf-8')
            result = subprocess.run([str(compiler), '--check-project-method-bodies', str(project)],
                                    cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            output = result.stdout + result.stderr
            if rejected:
                assert result.returncode != 0 and 'thread-local shared pointer' in output, (name, output)
            else:
                assert result.returncode == 0, (name, output)
            print(name, 'rejected' if rejected else 'accepted', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
