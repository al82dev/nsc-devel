"""Check calls on owners of borrowed fields against the binary SDK."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(part) for part in command], cwd=cwd,
                          capture_output=True, text=True, errors='replace')


def main():
    compiler, library = map(lambda value: Path(value).resolve(), sys.argv[1:3])
    metadata = library.with_suffix('.nsmeta')
    assert library.is_file() and metadata.is_file()
    with tempfile.TemporaryDirectory(prefix='nsc-field-method-borrow-') as temporary:
        root = Path(temporary)
        project = root / 'Consumer.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Consumer</Name><Language>3</Language>'
                           '</PropertyGroup><ItemGroup><Compile Include="Program.ns" />'
                           '<LibraryReference Include="' + library.as_posix() + '" Metadata="' +
                           metadata.as_posix() + '" /></ItemGroup></Project>', encoding='utf-8')
        source = root / 'Program.ns'
        for family in ('Rc', 'Arc'):
            common = '''using NativeSharp.Core;
class Payload { public long Data; public Payload() { Data = 42l; } }
class Holder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
delegate void Action(Holder item);
delegate void BoundAction();
class Outer { public Holder child; public Outer() { child = new Holder(); } }
struct InlineHolder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
class OuterValue { public InlineHolder child; }
'''.replace('STRONG', family).replace('MAKE', 'Make' + family)
            bodies = {
                'unsafe': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holder.value.Borrow(); holder.Replace(); return (int)view.Get().Data;''',
                'alias': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 borrowed Holder alias = holder; SharedView<Payload> view = holder.value.Borrow();
 alias.Replace(); return (int)view.Get().Data;''',
                'nested-unsafe': '''Outer holder = new Outer(); holder.child.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holder.child.value.Borrow(); holder.child.Replace(); return (int)view.Get().Data;''',
                'nested-struct-unsafe': '''OuterValue holder = new OuterValue();
 holder.child.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.child.value.Borrow();
 holder.child.Replace(); return (int)view.Get().Data;''',
                'nested-struct-independent': '''OuterValue first = new OuterValue(); OuterValue second = new OuterValue();
 first.child.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = first.child.value.Borrow();
 second.child.Replace(); return view.Get().Data == 42l ? 42 : 1;''',
                'indexed-unsafe': '''Holder[] holders = new Holder[1]; holders[0] = new Holder();
 holders[0].value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holders[0].value.Borrow();
 holders[0].Replace(); return (int)view.Get().Data;''',
                'indexed-independent': '''Holder[] first = new Holder[1]; Holder[] second = new Holder[1];
 first[0] = new Holder(); second[0] = new Holder(); first[0].value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = first[0].value.Borrow(); second[0].Replace();
 return view.Get().Data == 42l ? 42 : 1;''',
                'indexed-after-scope': '''Holder[] holders = new Holder[1]; holders[0] = new Holder();
 holders[0].value = SharedPointers.MAKE<Payload>(); { SharedView<Payload> view = holders[0].value.Borrow();
 if (view.Get().Data != 42l) return 1; } holders[0].Replace();
 return holders[0].value.Get().Data == 42l ? 42 : 2;''',
                'nested-independent': '''Outer first = new Outer(); Outer second = new Outer();
 first.child.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = first.child.value.Borrow();
 second.child.Replace(); return view.Get().Data == 42l ? 42 : 1;''',
                'nested-readonly': '''Outer holder = new Outer();
 holder.child.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.child.value.Borrow();
 return holder.child.value.Get().Data == 42l && view.Get().Data == 42l ? 42 : 1;''',
                'independent': '''Holder first = new Holder(); Holder second = new Holder();
 first.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = first.value.Borrow();
 second.Replace(); return view.Get().Data == 42l ? 42 : 1;''',
                'after-scope': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 { SharedView<Payload> view = holder.value.Borrow(); if (view.Get().Data != 42l) return 1; }
 holder.Replace(); return holder.value.Get().Data == 42l ? 42 : 2;''',
                'arg-borrowed': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holder.value.Borrow(); ReplaceBorrowed(holder); return (int)view.Get().Data;''',
                'arg-value': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holder.value.Borrow(); ReplaceValue(holder); return (int)view.Get().Data;''',
                'arg-alias': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 borrowed Holder alias = holder; SharedView<Payload> view = holder.value.Borrow();
 ReplaceBorrowed(alias); return (int)view.Get().Data;''',
                'arg-named': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holder.value.Borrow(); ReplaceNamed(item: holder, ignored: 1);
 return (int)view.Get().Data;''',
                'arg-independent': '''Holder first = new Holder(); Holder second = new Holder();
 first.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = first.value.Borrow();
 ReplaceBorrowed(second); return view.Get().Data == 42l ? 42 : 1;''',
                'arg-after-scope': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 { SharedView<Payload> view = holder.value.Borrow(); if (view.Get().Data != 42l) return 1; }
 ReplaceBorrowed(holder); return holder.value.Get().Data == 42l ? 42 : 2;''',
                'delegate-unsafe': '''Action action = ReplaceValue; Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.value.Borrow();
 action(holder); return (int)view.Get().Data;''',
                'delegate-independent': '''Action action = ReplaceValue; Holder first = new Holder(); Holder second = new Holder();
 first.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = first.value.Borrow();
 action(second); return view.Get().Data == 42l ? 42 : 1;''',
                'delegate-after-scope': '''Action action = ReplaceValue; Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); { SharedView<Payload> view = holder.value.Borrow();
 if (view.Get().Data != 42l) return 1; } action(holder); return holder.value.Get().Data == 42l ? 42 : 2;''',
                'bound-unsafe': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 BoundAction action = holder.Replace; SharedView<Payload> view = holder.value.Borrow();
 action(); return (int)view.Get().Data;''',
                'bound-shadowed': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 BoundAction action = holder.Replace; SharedView<Payload> view = holder.value.Borrow();
 { Holder holder = new Holder(); action(); } return (int)view.Get().Data;''',
                'bound-reassigned': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 BoundAction action = null; action = holder.Replace; SharedView<Payload> view = holder.value.Borrow();
 action(); return (int)view.Get().Data;''',
                'bound-copied': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 BoundAction original = holder.Replace; BoundAction action = original; SharedView<Payload> view = holder.value.Borrow();
 action(); return (int)view.Get().Data;''',
                'bound-reassigned-independent': '''Holder first = new Holder(); Holder second = new Holder();
 first.value = SharedPointers.MAKE<Payload>(); BoundAction action = null; action = second.Replace;
 SharedView<Payload> view = first.value.Borrow(); action(); return view.Get().Data == 42l ? 42 : 1;''',
                'bound-copied-finished': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 BoundAction original = holder.Replace; BoundAction action = original;
 { SharedView<Payload> view = holder.value.Borrow(); if (view.Get().Data != 42l) return 1; }
 action(); return holder.value.Get().Data == 42l ? 42 : 2;''',
                'bound-independent': '''Holder first = new Holder(); Holder second = new Holder();
 first.value = SharedPointers.MAKE<Payload>(); BoundAction action = second.Replace;
 SharedView<Payload> view = first.value.Borrow(); action(); return view.Get().Data == 42l ? 42 : 1;''',
                'bound-finished': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 BoundAction action = holder.Replace; { SharedView<Payload> view = holder.value.Borrow();
 if (view.Get().Data != 42l) return 1; } action(); return holder.value.Get().Data == 42l ? 42 : 2;''',
            }
            for name, body in bodies.items():
                helpers = ('class Program { public static void ReplaceBorrowed(borrowed Holder item) '
                           '{ item.value = SharedPointers.MAKE<Payload>(); } '
                           'public static void ReplaceValue(Holder item) '
                           '{ item.value = SharedPointers.MAKE<Payload>(); } '
                           'public static void ReplaceNamed(int ignored, borrowed Holder item) '
                           '{ item.value = SharedPointers.MAKE<Payload>(); } public static int Main() { ')
                source.write_text(common + (helpers + body + ' } }').replace('MAKE', 'Make' + family),
                                  encoding='utf-8')
                result = run([compiler, '--check-project-method-bodies', project], root)
                output = result.stdout + result.stderr
                if name in ('unsafe', 'alias', 'nested-unsafe', 'nested-struct-unsafe', 'indexed-unsafe',
                            'arg-borrowed', 'arg-value', 'arg-alias', 'arg-named', 'delegate-unsafe', 'bound-unsafe',
                            'bound-shadowed', 'bound-reassigned', 'bound-copied'):
                    assert result.returncode != 0 and 'borrow' in output, (family, name, output)
                    print(family, name, 'rejected', flush=True)
                    continue
                assert result.returncode == 0, (family, name, output)
                for level in ('O0', 'O2'):
                    executable = root / (family + '-' + name + '-' + level + '.exe')
                    result = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                                  '-o', executable, '--no-artifact-cache'], root)
                    assert result.returncode == 0, (family, name, level, result.stdout + result.stderr)
                    result = run([executable], root)
                    assert result.returncode == 42, (family, name, level, result.returncode)
                    print(family, name, level, 'exit=42', flush=True)
            for name, invocation in (('implicit', 'Replace();'), ('explicit', 'this.Replace();'),
                                     ('finished', 'if (view.Get().Data != 42l) return 1; } Replace();')):
                opening = '{ ' if name == 'finished' else ''
                work = ('value = SharedPointers.MAKE<Payload>(); ' + opening +
                        'SharedView<Payload> view = value.Borrow(); ' + invocation +
                        (' return value.Get().Data == 42l ? 42 : 2;' if name == 'finished'
                         else ' return (int)view.Get().Data;'))
                source.write_text((common + 'class SelfHolder { public STRONG<Payload> value; '
                    'public void Replace() { value = SharedPointers.MAKE<Payload>(); } '
                    'public int Work() { ' + work + ' } } class Program { public static int Main() { '
                    + ('SelfHolder holder = new SelfHolder(); return holder.Work();' if name == 'finished'
                       else 'return 0;') + ' } }').replace('STRONG', family).replace('MAKE', 'Make' + family),
                    encoding='utf-8')
                result = run([compiler, '--check-project-method-bodies', project], root)
                output = result.stdout + result.stderr
                if name != 'finished':
                    assert result.returncode != 0 and 'borrow' in output, (family, name, output)
                    print(family, 'this-' + name, 'rejected', flush=True)
                    continue
                assert result.returncode == 0, (family, name, output)
                for level in ('O0', 'O2'):
                    executable = root / (family + '-this-finished-' + level + '.exe')
                    result = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                                  '-o', executable, '--no-artifact-cache'], root)
                    assert result.returncode == 0, (family, name, level, result.stdout + result.stderr)
                    result = run([executable], root)
                    assert result.returncode == 42, (family, name, level, result.returncode)
                    print(family, 'this-finished', level, 'exit=42', flush=True)
            event_bodies = {
                'unsafe': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holder.value.Borrow(); Changed(holder); return (int)view.Get().Data;''',
                'independent': '''Holder first = new Holder(); Holder second = new Holder();
 first.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = first.value.Borrow();
 Changed(second); return view.Get().Data == 42l ? 42 : 1;''',
                'finished': '''Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 { SharedView<Payload> view = holder.value.Borrow(); if (view.Get().Data != 42l) return 1; }
 Changed(holder); return holder.value.Get().Data == 42l ? 42 : 2;''',
            }
            for name, body in event_bodies.items():
                source.write_text((common + '''class Publisher { public event Action Changed;
 public static void Replace(Holder item) { item.value = SharedPointers.MAKE<Payload>(); }
 public int Work() { Changed += Replace; ''' + body + ''' } }
class Program { public static int Main() { ''' +
                    ('return 0;' if name == 'unsafe' else 'Publisher publisher = new Publisher(); return publisher.Work();') +
                    ' } }').replace('MAKE', 'Make' + family), encoding='utf-8')
                result = run([compiler, '--check-project-method-bodies', project], root)
                output = result.stdout + result.stderr
                if name == 'unsafe':
                    assert result.returncode != 0 and 'borrow' in output, (family, name, output)
                    print(family, 'event-unsafe rejected', flush=True)
                    continue
                assert result.returncode == 0, (family, name, output)
                for level in ('O0', 'O2'):
                    executable = root / (family + '-event-' + name + '-' + level + '.exe')
                    result = run([compiler, '--emit-project-oop-exe', project, '-' + level,
                                  '-o', executable, '--no-artifact-cache'], root)
                    assert result.returncode == 0, (family, name, level, result.stdout + result.stderr)
                    result = run([executable], root)
                    assert result.returncode == 42, (family, name, level, result.returncode)
                    print(family, 'event-' + name, level, 'exit=42', flush=True)


if __name__ == '__main__':
    main()
