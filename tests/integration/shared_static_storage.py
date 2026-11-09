import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(item) for item in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler, repo = map(Path, sys.argv[1:])
    names = ('SharedStorage', 'SharedControlStorage', 'SharedReferenceCounts', 'SharedLocalControl',
             'SharedAtomicControl', 'SharedBorrow', 'SharedLocalHandle', 'SharedLocalWeak',
             'SharedLocalFactory', 'SharedAtomicHandle', 'SharedAtomicWeak', 'SharedAtomicFactory',
             'SharedView', 'Rc', 'WeakRc', 'Arc', 'WeakArc', 'SharedPointers')
    cases = {
        'field': ('class Holder { public static WeakRc<Payload> value; }', True),
        'container': ('struct Box { public Rc<Payload> value; } class Holder { public static Box value; }', True),
        'generic_container': ('struct Box<T> { public T value; } class Holder { public static Box<WeakRc<Payload>> value; }', True),
        'atomic_container': ('class Holder { public static Arc<WeakRc<Payload>> value; }', True),
        'base_container': ('class Base { public WeakRc<Payload> value; } class Derived : Base { } '
                           'class Holder { public static Derived value; }', True),
        'polymorphic_container': ('class Base { } class Derived : Base { public WeakRc<Payload> value; } '
                                  'class Holder { public static Base value; }', True),
        'interface_container': ('interface IValue { } class Derived : IValue { public WeakRc<Payload> value; } '
                                'class Holder { public static IValue value; }', True),
        'array': ('class Holder { public static Rc<Payload>[] value; }', True),
        'fixed_array': ('class Holder { public static Rc<Payload>[2] value; }', True),
        'pointer': ('class Holder { public static WeakRc<Payload>* value; }', True),
        'generic_static_field': ('class Box<T> { public static T value; } '
                                 'class Holder { public Box<WeakRc<Payload>> value; }', True),
        'local_static': ('', True),
        'namespace': ('namespace Global { public WeakRc<Payload> value; }', True),
        'atomic': ('class Holder { public static WeakArc<Payload> value; }', False),
        'local': ('', False),
    }
    with tempfile.TemporaryDirectory(prefix='nsc-shared-static-projects-') as temporary:
        root = Path(temporary)
        producer = root / 'producer'
        consumer = root / 'consumer'
        producer.mkdir()
        consumer.mkdir()
        for name in names:
            shutil.copyfile(repo / 'lib' / 'NativeSharp.Core' / (name + '.ns'), producer / (name + '.ns'))
        (producer / 'Exception.ns').write_text('namespace NativeSharp.Core { public class Exception { } }',
                                               encoding='utf-8')
        (producer / 'Producer.nsproj').write_text(
            '<Project><PropertyGroup><Name>Producer</Name><Language>3</Language></PropertyGroup>'
            '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        project = consumer / 'Consumer.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Consumer</Name><Language>3</Language></PropertyGroup>'
            '<ItemGroup><Compile Include="Main.ns" /><ProjectReference Include="../producer/Producer.nsproj" />'
            '</ItemGroup></Project>', encoding='utf-8')
        for name, (declarations, rejected) in cases.items():
            local = 'static WeakRc<Payload> value;' if name == 'local_static' else (
                'Rc<Payload> value = SharedPointers.MakeRc<Payload>();' if name == 'local' else '')
            source = ('using NativeSharp.Core; class Payload { } ' + declarations +
                      ' class Program { public static int Main() { ' + local + ' return 42; } }')
            (consumer / 'Main.ns').write_text(source, encoding='utf-8')
            check = run([compiler, '--check-project-method-bodies', project], consumer)
            output = check.stdout + check.stderr
            if rejected:
                assert check.returncode != 0 and 'thread-local shared pointer' in output, (name, output)
                print(name, 'rejected', flush=True)
            else:
                assert check.returncode == 0, (name, output)
                print(name, 'accepted', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
