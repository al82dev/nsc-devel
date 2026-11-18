"""An operator must not retain an object owned by an `in` operand."""
from pathlib import Path
import subprocess
import sys
import tempfile


PREFIX = '''class Payload { public int Value; public Payload() { Value = 42; } }
class Registry {
 public static Payload Saved;
 public static void Store(Payload value) { Saved = value; }
}
struct Owner {
 public owned Payload item;
 public Owner(int value) { item = new Payload(); }
 public static int operator +(in Owner a, in Owner b) { BODY }
}
class Program { public static int Main() { return new Owner(1) + new Owner(2); } }
'''
CASES = {
    'read-scalar': ('return a.item.Value;', True),
    'store-static': ('Registry.Saved = a.item; return 42;', False),
    'store-through-local': ('Payload alias = a.item; Registry.Saved = alias; return 42;', False),
    'store-through-call': ('Registry.Store(a.item); return 42;', False),
    'store-through-borrowed-local': (
        'borrowed Payload alias = a.item; Registry.Saved = alias; return 42;', False),
    'store-through-borrowed-call': (
        'borrowed Payload alias = a.item; Registry.Store(alias); return 42;', False),
    'mutate-class': ('a.item.Value = 9; return 42;', False),
}


def main():
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-in-operator-static-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>OperatorStaticEscape</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
        for name, (body, valid) in CASES.items():
            (root / 'Main.ns').write_text(PREFIX.replace('BODY', body), encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                built = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                       errors='replace', timeout=120)
                output = built.stdout + built.stderr
                print('COMMAND', subprocess.list2cmdline(command), 'EXIT', built.returncode, output, flush=True)
                if not valid:
                    if built.returncode == 0 or not any(
                            word in output for word in ('borrowed', 'readonly', 'not assignable')):
                        raise AssertionError((name, level, built.returncode, output))
                    continue
                if built.returncode:
                    raise AssertionError((name, level, output))
                result = subprocess.run([str(executable)], cwd=root, capture_output=True,
                                        text=True, errors='replace', timeout=120)
                print('EXECUTE', name, level, 'EXIT', result.returncode, flush=True)
                if result.returncode != 42:
                    raise AssertionError((name, level, result.returncode))
    print('operator_in_temporary_static_escape: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
