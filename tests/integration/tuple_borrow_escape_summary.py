"""Check published escape summaries for tuple parameters carrying borrowed views."""
from pathlib import Path
import subprocess
import sys
import tempfile


def field(data, cursor):
    colon = data.index(':', cursor)
    end = colon + 1 + int(data[cursor:colon])
    if data[end] != ';':
        raise AssertionError('invalid manifest field')
    return data[colon + 1:end], end + 1


def items(data):
    cursor = 0
    while cursor < len(data):
        value, cursor = field(data, cursor)
        yield value


def run(command, cwd):
    result = subprocess.run([str(x) for x in command], cwd=cwd, capture_output=True,
                            text=True, errors='replace', timeout=120)
    if result.returncode:
        raise AssertionError(f'{command}: exit {result.returncode}\n{result.stdout}{result.stderr}')
    return result.stdout


def main():
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='tuple-escape-summary-') as temporary:
        root = Path(temporary)
        (root / 'Main.ns').write_text('''public borrowed struct View { public int Value; }
public class Owner { public View Borrow() { return default(View); } }
public class Reader {
 public static View FromOwner(Owner owner) { return owner.Borrow(); }
 public static int ReadOwner(Owner owner) { return 42; }
 public static (View, int) Echo((View, int) value) { return value; }
 public static (View, int) EchoAlias((View, int) value) { (View, int) copy = value; return copy; }
 public static (View, int) EchoCall((View, int) value) { return Echo(value); }
 public static (View, int) EchoCallAlias((View, int) value) { (View, int) copy = value; return Echo(copy); }
 public static (View, int) EchoSecond((View, int) first, (View, int) second) { (View, int) copy = second; return copy; }
 public static (View, int) DirectSecond((View, int) first, (View, int) second) { return second; }
 public static View ExtractSecond((View, int) first, (View, int) second) { var (view, n) = second; return view; }
 public static View DirectView((View, int) first, View second) { return second; }
 public static View ReassignedView((View, int) first, View second, View third) { second = third; return second; }
 public static View DirectReassigned(View first, View second) { second = first; return second; }
 public static ((View, int), int) EchoNested(((View, int), int) value) { return value; }
 public static int Read((View, int) value) { return 42; }
 public static int ReadForward((View, int) value) { return Read(value); }
 public static (int, int) PlainEcho((int, int) value) { return value; }
}''', encoding='utf-8')
        project = root / 'Producer.nsproj'
        project.write_text('<Project><PropertyGroup><Name>TupleSummary</Name>'
            '<OutputType>Library</OutputType></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
            '</ItemGroup></Project>', encoding='utf-8')
        library = root / 'Producer.lib'
        run([compiler, '--emit-project-oop-lib', project, '-O0', '-o', library, '--no-artifact-cache'], root)
        output = run([compiler, '--dump-api-surface', library.with_suffix('.nsmeta')], root)
        data = next(line for line in output.splitlines() if line.startswith('25:NativeSharp.source-api.v2;'))
        _, cursor = field(data, 0)
        count, cursor = field(data, cursor)
        found = {}
        for _ in range(int(count)):
            key, cursor = field(data, cursor)
            contract, cursor = field(data, cursor)
            if not key.startswith('member:type:TupleSummary::Reader:2:'):
                continue
            parts = iter(items(contract))
            prefix = [next(parts) for _ in range(10)]
            parameter_count = int(next(parts))
            for _ in range(parameter_count * 14): next(parts)
            next(parts)  # return lifetime
            for _ in range(int(next(parts))): next(parts)
            for _ in range(int(next(parts)) * 2): next(parts)
            escapes = [int(next(parts)) for _ in range(int(next(parts)))]
            next(parts)  # FFI contract
            if list(parts): raise AssertionError('unexpected member contract fields')
            found[prefix[3]] = escapes
        expected = {'FromOwner': [1], 'ReadOwner': [0], 'Echo': [1], 'EchoAlias': [1], 'EchoCall': [1], 'EchoCallAlias': [1],
                    'EchoSecond': [1, 1], 'DirectSecond': [1, 1], 'ExtractSecond': [1, 1],
                    'DirectView': [1, 1], 'ReassignedView': [1, 1, 1], 'DirectReassigned': [1, 1], 'EchoNested': [1],
                    'Read': [0], 'ReadForward': [0], 'PlainEcho': [0]}
        if found != expected:
            raise AssertionError(f'escape summaries: {found!r}, expected {expected!r}')
        print('tuple return and nonescaping controls:', found, flush=True)
        unsafe_cases = {
            'direct': '''public borrowed struct View { public int Value; }
public class Sink { public static (View, int) saved;
 public static void Save((View, int) value) { saved = value; } }''',
            'alias': '''public borrowed struct View { public int Value; }
public class Sink { public static (View, int) saved;
 public static void Save((View, int) value) { (View, int) copy = value; saved = copy; } }''',
            'nested': '''public borrowed struct View { public int Value; }
public class Sink { public static ((View, int), int) saved;
 public static void Save(((View, int), int) value) { saved = value; } }''',
        }
        for name, unsafe_source in unsafe_cases.items():
            (root / 'Main.ns').write_text(unsafe_source, encoding='utf-8')
            result = subprocess.run([str(compiler), '--emit-project-oop-lib', str(project), '-O0',
                                     '-o', str(root / ('Unsafe-' + name + '.lib')), '--no-artifact-cache'],
                                    cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            output = result.stdout + result.stderr
            if result.returncode == 0 or 'tuple containing a borrowed view cannot be stored outside a tracked local' not in output:
                raise AssertionError(f'unsafe tuple storage {name}: exit {result.returncode}\n{output}')
            print('tuple global storage rejected', name, ':', output, flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
