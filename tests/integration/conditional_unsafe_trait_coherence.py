"""Check coherence of conditional thread-marker impl declarations."""
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1]).resolve()
    prefix = '''trait Send {} trait Sync {}
namespace Alpha { public trait Send {} public trait Sync {} }
namespace Beta { public trait Send {} public trait Sync {} }
struct Box<T> { public T* value; }
'''
    conditional = 'unsafe impl<T> Send for Box<T> where T : Send, Sync {}\n'
    unconditional = 'unsafe impl<T> Send for Box<T> {}\n'
    cases = {
        'duplicate-conditional': (conditional + conditional, False),
        'conditional-first': (conditional + unconditional, False),
        'unconditional-first': (unconditional + conditional, False),
        'distinct-homonyms': ('''unsafe impl<T> Alpha.Send for Box<T> where T : Alpha.Send, Alpha.Sync {}
unsafe impl<T> Beta.Send for Box<T> where T : Beta.Send, Beta.Sync {}
''', True),
        'distinct-markers': (conditional +
            'unsafe impl<T> Sync for Box<T> where T : Send, Sync {}\n', True),
    }
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-conditional-coherence-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>CoherenceProbe</Name>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                           '</ItemGroup></Project>', encoding='utf-8')
        for name, (declarations, expected) in cases.items():
            (root / 'Main.ns').write_text(prefix + declarations +
                'class Program { public static int Main() { return 42; } }\n', encoding='utf-8')
            result = subprocess.run([str(compiler), '--check-project-generics', str(project)],
                                    cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
            output = result.stdout + result.stderr
            accepted = result.returncode == 0
            print(name, 'accepted=' + str(accepted), 'expected=' + str(expected), flush=True)
            if accepted != expected or (not expected and 'duplicate' not in output):
                failures.append((name, result.returncode, output))
        (root / 'Main.ns').write_text(prefix +
            'unsafe impl<T> Missing.Send for Box<T> where T : Send, Sync {}\n' +
            'class Program { public static int Main() { return 42; } }\n', encoding='utf-8')
        result = subprocess.run([str(compiler), '--check-project-generics', str(project)],
                                cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
        print('unresolved-single-marker', 'accepted=' + str(result.returncode == 0), 'expected=False', flush=True)
        if result.returncode == 0 or 'cannot resolve a unique trait identity' not in (result.stdout + result.stderr):
            failures.append(('unresolved-single-marker', result.returncode, result.stdout + result.stderr))
        (root / 'Main.ns').write_text(prefix +
            'unsafe impl<T> Send for Box<T> where T : Send, Missing.Sync {}\n' +
            'class Program { public static int Main() { return 42; } }\n', encoding='utf-8')
        result = subprocess.run([str(compiler), '--check-project-generics', str(project)],
                                cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
        print('unresolved-condition', 'accepted=' + str(result.returncode == 0), 'expected=False', flush=True)
        if result.returncode == 0 or 'cannot resolve a unique trait identity' not in (result.stdout + result.stderr):
            failures.append(('unresolved-condition', result.returncode, result.stdout + result.stderr))
        (root / 'Main.ns').write_text('''class Send {} trait Sync {}
struct Box<T> { public T* value; }
unsafe impl<T> Send for Box<T> where T : Send, Sync {}
class Program { public static int Main() { return 42; } }
''', encoding='utf-8')
        result = subprocess.run([str(compiler), '--check-project-generics', str(project)],
                                cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
        print('class-as-marker', 'accepted=' + str(result.returncode == 0), 'expected=False', flush=True)
        if result.returncode == 0 or 'conditional thread-safety impl must name a trait or interface' not in (result.stdout + result.stderr):
            failures.append(('class-as-marker', result.returncode, result.stdout + result.stderr))
        (root / 'Main.ns').write_text('''trait Send {} class Sync {}
struct Box<T> { public T* value; }
unsafe impl<T> Send for Box<T> where T : Send, Sync {}
class Program { public static int Main() { return 42; } }
''', encoding='utf-8')
        result = subprocess.run([str(compiler), '--check-project-generics', str(project)],
                                cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
        print('class-as-condition', 'accepted=' + str(result.returncode == 0), 'expected=False', flush=True)
        if result.returncode == 0 or 'conditional thread-safety constraint must name a trait or interface' not in (result.stdout + result.stderr):
            failures.append(('class-as-condition', result.returncode, result.stdout + result.stderr))
        split = root / 'split'
        split.mkdir()
        (split / 'Types.ns').write_text('''namespace Alpha { public trait Send {} public trait Sync {} }
namespace Beta { public trait Send {} public trait Sync {} }
struct Box<T> { public T* value; }
''' +
            'class Program { public static int Main() { return 42; } }\n', encoding='utf-8')
        for name, namespace in (('First', 'Alpha'), ('Second', 'Beta')):
            (split / (name + '.ns')).write_text('using ' + namespace + ';\n' + conditional,
                                                encoding='utf-8')
        split_project = split / 'Probe.nsproj'
        split_project.write_text('<Project><PropertyGroup><Name>SplitCoherence</Name>'
                                 '</PropertyGroup><ItemGroup><Compile Include="*.ns" />'
                                 '</ItemGroup></Project>', encoding='utf-8')
        result = subprocess.run([str(compiler), '--check-project-generics', str(split_project)],
                                cwd=split, capture_output=True, text=True, errors='replace', timeout=120)
        print('split-homonyms', 'accepted=' + str(result.returncode == 0), 'expected=True', flush=True)
        if result.returncode != 0:
            failures.append(('split-homonyms', result.returncode, result.stdout + result.stderr))
    for failure in failures:
        print('FAIL', *failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
