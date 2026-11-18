"""Keep view-returning operators from lending through temporary operands."""
from pathlib import Path
import subprocess
import sys
import tempfile


TYPES = '''borrowed struct View<lifetime a, T> {
 private borrowed[a] T* target;
 public View(ref[a] T value) { unsafe("view") { target = &value; } }
 public ref[this] T Get() { unsafe("view") { return ref *target; } }
}
borrowed struct Owner<lifetime a> {
 public View<int> Field;
 public Owner(ref[a] int value) { Field = new View<int>(ref value); }
 public static View<int> operator +(in Owner a, in Owner b) { return a.Field; }
}
'''
CASES = {
    'local-owner': ('''class Program { public static int Main() { int value = 42;
 Owner a = new Owner(ref value); View<int> view = a + a; return view.Get(); } }''', True),
    'escaping-temporary': ('''class Program { public static View<int> Leak() { int value = 42;
 return new Owner(ref value) + new Owner(ref value); } public static int Main() { return 42; } }''', False),
}


def main():
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-in-operator-view-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>OperatorView</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
        for name, (body, valid) in CASES.items():
            (root / 'Main.ns').write_text(TYPES + body, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / (name + '-' + level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                           '-o', str(executable), '--no-artifact-cache']
                built = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                       errors='replace', timeout=120)
                output = built.stdout + built.stderr
                print('COMMAND', subprocess.list2cmdline(command), 'EXIT', built.returncode, output, flush=True)
                if not valid:
                    if built.returncode == 0 or 'no viable' not in output:
                        raise AssertionError((name, level, built.returncode, output))
                    continue
                if built.returncode:
                    raise AssertionError((name, level, output))
                run = subprocess.run([str(executable)], cwd=root, capture_output=True,
                                     text=True, errors='replace', timeout=120)
                print('EXECUTE', name, level, 'EXIT', run.returncode, run.stdout, run.stderr, flush=True)
                if run.returncode != 42:
                    raise AssertionError((name, level, run.returncode))
    print('operator_in_temporary_view: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
