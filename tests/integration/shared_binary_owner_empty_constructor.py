"""Check empty shared fields and unrelated value fields on failed construction."""
from pathlib import Path
import sys
import tempfile

from shared_binary_owner_constructor_failure import reference, run


PRODUCER = '''using NativeSharp.Core;
public class Payload { public Payload() { } }
public class Counter { public static int Drops; public static int Read() { return Drops; } }
public struct Sentinel { ~Sentinel() { Counter.Drops = Counter.Drops + 1; } }
class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
public class Holder { public FAMILY<Payload> value; public Sentinel unrelated; public int number;
 public Holder() { number = Gate.Read(); } }
'''
CONSUMER = '''using NativeSharp.Core;
class Program { public static int Main() { int caught = 0;
 try { Holder holder = new Holder(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Counter.Read() == 0 ? 42 : 1;
} }
'''


def main():
    compiler, core, metadata = (Path(item).resolve() for item in sys.argv[1:4])
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-binary-owner-empty-') as temporary:
        root = Path(temporary)
        producer = root / 'Producer.nsproj'
        consumer = root / 'Consumer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>SharedEmptyProducer</Name>'
            '<OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup><Compile Include="Producer.ns"/>' +
            reference(core, metadata) + '</ItemGroup></Project>', encoding='utf-8')
        (root / 'Main.ns').write_text(CONSUMER, encoding='utf-8')
        for family in ('Rc', 'WeakRc', 'Arc', 'WeakArc'):
            (root / 'Producer.ns').write_text(PRODUCER.replace('FAMILY', family), encoding='utf-8')
            for producer_level in ('O0', 'O2'):
                name = family + '-' + producer_level
                library = root / (name + '.lib')
                if run([compiler, '--emit-project-oop-lib', producer, '-' + producer_level,
                        '-o', library, '--no-artifact-cache'], root).returncode:
                    failures.append((name, 'producer'))
                    continue
                consumer.write_text('<Project><PropertyGroup><Name>SharedEmptyConsumer</Name></PropertyGroup>'
                    '<ItemGroup><Compile Include="Main.ns"/>' + reference(core, metadata) +
                    reference(library, library.with_suffix('.nsmeta')) + '</ItemGroup></Project>', encoding='utf-8')
                for level in ('O0', 'O2'):
                    executable = root / (name + '-' + level + '.exe')
                    if run([compiler, '--emit-project-oop-exe', consumer, '-' + level,
                            '-o', executable, '--no-artifact-cache'], root).returncode:
                        failures.append((name, level, 'compile'))
                        continue
                    result = run([executable], root)
                    if result.returncode != 42:
                        failures.append((name, level, 'execute', result.returncode))
    print('FAILURES', failures, flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
