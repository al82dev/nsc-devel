"""Check readonly Arc payload access with the isolated conditional Core binary."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, cwd):
    argv = [str(value) for value in command]
    result = subprocess.run(argv, cwd=cwd, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline(argv), 'EXIT', result.returncode,
          '\n', result.stdout, result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    library = Path(sys.argv[2]).resolve()
    preamble = '''using NativeSharp.Core;
class Child { public int Data; public Child() { Data = 42; } public void Change() { Data = 9; } }
class Payload { public int Data; public int[] Items; public Child Item;
 public Payload() { Data = 42; Items = new int[1]; Items[0] = 42; Item = new Child(); }
 public void Change() { Data = 9; } }
class Program { public static Payload Escaped;
 static int RequireSend<T>() where T : Send { return 1; }
 static int RequireSync<T>() where T : Sync { return 1; }
 static void ChangeChild(Child child) { child.Data = 9; }
 static int ReadDirect(Arc<Payload> value) { borrowed Child child = value.Get().Item; return child.Data; }
 static int ReadView(Arc<Payload> value) { SharedView<Payload> view = value.Borrow();
  borrowed Child child = view.Get().Item; return child.Data; }
 static int ReadMixed(Arc<Payload> value) { Arc<Payload> local = SharedPointers.MakeArc<Payload>();
  SharedView<Payload> view = false ? value.Borrow() : local.Borrow();
  borrowed Child child = view.Get().Item; return child.Data; }
 public static int Main() { Arc<Payload> owner = SharedPointers.MakeArc<Payload>();
 if (RequireSend<Arc<Payload>>() != 1 || RequireSync<Arc<Payload>>() != 1) return 3;
 BODY } }
'''
    rejected = {
        'direct': ('owner.Get().Data = 9; return 0;', 'Data'),
        'alias': ('borrowed Payload value = owner.Get(); value.Data = 9; return 0;', 'value.Data'),
        'view-alias': ('SharedView<Payload> view = owner.Borrow(); '
                       'borrowed Payload value = view.Get(); value.Data = 9; return 0;', 'value.Data'),
        'array': ('int[] values = owner.Get().Items; values[0] = 9; return 0;', 'values[0]'),
        'method': ('borrowed Payload value = owner.Get(); value.Change(); return 0;', 'value.Change'),
        'escape': ('Escaped = owner.Get(); return 0;', 'Escaped'),
        'nested-direct': ('owner.Get().Item.Data = 9; return 0;', 'owner.Get().Item.Data'),
        'conditional-direct': ('(true ? owner.Get() : owner.Get()).Data = 9; return 0;',
                               'owner.Get()'),
        'cast-direct': ('((Payload)owner.Get()).Data = 9; return 0;', 'owner.Get()'),
        'nested-method': ('owner.Get().Item.Change(); return 0;', 'owner.Get().Item.Change'),
        'nested-argument': ('ChangeChild(owner.Get().Item); return 0;', 'owner.Get().Item'),
        'nested-alias': ('Child child = owner.Get().Item; child.Data = 9; return 0;', 'owner.Get().Item'),
        'nested-local-mutate': ('borrowed Payload parent = owner.Get(); borrowed Child child = parent.Item; '
                                'child.Data = 9; return 0;', 'child.Data'),
        'nested-local-call': ('borrowed Payload parent = owner.Get(); borrowed Child child = parent.Item; '
                              'child.Change(); return 0;', 'child.Change'),
        'nested-direct-local-mutate': ('borrowed Child child = owner.Get().Item; '
                                       'child.Data = 9; return 0;', 'child.Data'),
        'nested-direct-local-call': ('borrowed Child child = owner.Get().Item; '
                                     'child.Change(); return 0;', 'child.Change'),
        'nested-view-local-mutate': ('SharedView<Payload> view = owner.Borrow(); '
                                     'borrowed Child child = view.Get().Item; child.Data = 9; return 0;', 'child.Data'),
        'nested-view-multiple-first': ('Arc<Payload> other = SharedPointers.MakeArc<Payload>(); '
            'SharedView<Payload> view = true ? owner.Borrow() : other.Borrow(); '
            'borrowed Child child = view.Get().Item; owner = default(Arc<Payload>); return child.Data;', 'owner'),
        'nested-view-multiple-second': ('Arc<Payload> other = SharedPointers.MakeArc<Payload>(); '
            'SharedView<Payload> view = false ? owner.Borrow() : other.Borrow(); '
            'borrowed Child child = view.Get().Item; other = default(Arc<Payload>); return child.Data;', 'other'),
        'nested-view-reassigned-original': ('Arc<Payload> other = SharedPointers.MakeArc<Payload>(); '
            'SharedView<Payload> view = owner.Borrow(); borrowed Child child = view.Get().Item; '
            'view = other.Borrow(); owner = default(Arc<Payload>); return child.Data;', 'owner'),
        'nested-view-reassigned-current': ('Arc<Payload> other = SharedPointers.MakeArc<Payload>(); '
            'SharedView<Payload> view = owner.Borrow(); view = other.Borrow(); '
            'borrowed Child child = view.Get().Item; other = default(Arc<Payload>); return child.Data;', 'other'),
    }
    with tempfile.TemporaryDirectory(prefix='nsc-arc-conditional-readonly-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>ArcReadonly</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                           '<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                           library.with_suffix('.nsmeta').as_posix() + '" />'
                           '</ItemGroup></Project>', encoding='utf-8')
        main_source = root / 'Main.ns'
        for name, (body, site) in rejected.items():
            main_source.write_text(preamble.replace('BODY', body), encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', project], root)
            output = result.stdout + result.stderr
            print(name, 'rejected=' + str(result.returncode != 0), flush=True)
            print(output, flush=True)
            if (result.returncode == 0 or site not in output or 'error:' not in output or
                    not any(word in output.lower() for word in ('readonly', 'borrow', 'assignable'))):
                print('FAIL', name, output, file=sys.stderr)
                return 1
        for name, body in (
                ('constructor', 'public Payload() { Alias = this; }'),
                ('destructor', 'public Payload() {} ~Payload() { Alias = this; }')):
            source = '''using NativeSharp.Core;
class Payload { public static Payload Alias; BODY }
class Program { static int Require<T>() where T : Send { return 1; }
 public static int Main() { Arc<Payload> owner = SharedPointers.MakeArc<Payload>();
 return Require<Arc<Payload>>() == 1 ? 42 : 1; } }
'''.replace('BODY', body)
            main_source.write_text(source, encoding='utf-8')
            for level in ('O0', 'O2'):
                result = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                              '-o', root / (name + '-' + level + '.exe'), '--no-artifact-cache'], root)
                output = result.stdout + result.stderr
                print(name, level, 'rejected=' + str(result.returncode != 0), flush=True)
                if result.returncode == 0 or 'shared payload publishes its receiver' not in output:
                    print('FAIL', name, level, output, file=sys.stderr)
                    return 1
        valid = {
            'conditional-read': 'return (true ? owner.Get() : owner.Get()).Data;',
            'cast-read': 'return ((Payload)owner.Get()).Data;',
            'local': 'borrowed Payload parent = owner.Get(); borrowed Child child = parent.Item; return child.Data;',
            'direct': 'borrowed Child child = owner.Get().Item; return child.Data;',
            'view': 'SharedView<Payload> view = owner.Borrow(); borrowed Child child = view.Get().Item; return child.Data;',
            'multiple': 'Arc<Payload> other = SharedPointers.MakeArc<Payload>(); '
                        'SharedView<Payload> view = false ? owner.Borrow() : other.Borrow(); '
                        'borrowed Child child = view.Get().Item; return child.Data;',
            'parameter-direct': 'return ReadDirect(SharedPointers.MakeArc<Payload>());',
            'parameter-view': 'return ReadView(SharedPointers.MakeArc<Payload>());',
            'parameter-mixed': 'return ReadMixed(SharedPointers.MakeArc<Payload>());',
            'reassigned-view': 'Arc<Payload> other = SharedPointers.MakeArc<Payload>(); '
                'SharedView<Payload> view = owner.Borrow(); view = other.Borrow(); '
                'borrowed Child child = view.Get().Item; return child.Data;',
        }
        for name, body in valid.items():
            main_source.write_text(preamble.replace('BODY', body), encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = root / ('valid-' + name + '-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', executable,
                              '--no-artifact-cache'], root)
                if result.returncode != 0:
                    print('FAIL valid compile', name, level, result.stdout + result.stderr, file=sys.stderr)
                    return 1
                result = run([executable], root)
                print('valid', name, level, 'exit=' + str(result.returncode), flush=True)
                if result.returncode != 42:
                    print('FAIL valid run', name, level, result.returncode, result.stdout + result.stderr, file=sys.stderr)
                    return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
