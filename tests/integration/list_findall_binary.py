"""Build the current Core/Collections libraries and consume FindAll without sources."""
import argparse
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--linker', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--libraries', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    root = args.work_dir.resolve()
    root.mkdir(parents=True, exist_ok=False)
    repo = Path(__file__).resolve().parent.parent.parent
    env = os.environ.copy()
    env.update(TEMP=str(root), TMP=str(root), NSC_LINKER=str(args.linker.resolve()),
               NSC_WINDOWS_RUNTIME_LIBRARY=str(args.runtime.resolve()))
    compiler = str(args.compiler.resolve())

    def compile_project(project, output, action, cache, optimization, library_dir):
        result = subprocess.run([compiler, optimization, '--artifact-cache=' + str(root / cache),
                                 '--library-reference-root=' + str(library_dir),
                                 action, str(project), '-o', str(output)], cwd=root, env=env,
                                capture_output=True, text=True, timeout=120)
        if result.returncode:
            raise RuntimeError(f'{project.name}: {result.stdout} {result.stderr}')

    libraries = args.libraries.resolve() / 'stdlib'
    for opt in ('-O0', '-O1', '-O2', '-O3'):
        library_dir = root / opt[1:]
        library_dir.mkdir()
        core = library_dir / 'NativeSharpCore.lib'
        compile_project(libraries / 'NativeSharp.Core/NativeSharp.Core.nsproj', core,
                        '--emit-project-oop-lib', opt + '-core-cache', opt, library_dir)
        generic = library_dir / 'NativeSharpCoreCollectionsGeneric.lib'
        compile_project(libraries / 'NativeSharp.Core.Collections.Generic/NativeSharp.Core.Collections.Generic.nsproj',
                        generic, '--emit-project-oop-lib', opt + '-generic-cache', opt, library_dir)
    consumer = root / 'Consumer.nsproj'
    consumer.write_text('<Project><PropertyGroup><Name>Consumer</Name><OutputType>Exe</OutputType>'
                        '</PropertyGroup><ItemGroup><Compile Include="Main.ns"/>'
                        '<LibraryReference Include="NativeSharpCore.lib" Metadata="NativeSharpCore.nsmeta"/>'
                        '<LibraryReference Include="NativeSharpCoreCollectionsGeneric.lib" Metadata="NativeSharpCoreCollectionsGeneric.nsmeta"/>'
                        '</ItemGroup></Project>', encoding='utf-8')
    normal = (repo / 'tests/cases/libraries/list_findall_effects/Main.ns').read_text(encoding='utf-8')
    mutation = ('using NativeSharp.Core.Collections.Generic; public class Tracker {'
                ' public static List<int> Target; public static bool Mutate(in int x) {'
                ' Target.Clear(); return true; } } public class Program { public static int Main() {'
                ' Tracker.Target = new List<int>(); Tracker.Target.Add(1);'
                ' InPredicate<int> p = (in int x) => Tracker.Mutate(in x);'
                ' int[] a = Tracker.Target.FindAll(p); delete[] a; return 91; } }')
    exceptional = ('using NativeSharp.Core; using NativeSharp.Core.Collections.Generic; '
                   'public class Gate { static Gate() throws Exception { throw new Exception(); } '
                   'public static int Read() { return 1; } } public class Program { '
                   'public static int Main() { raii List<int> a = new List<int>(); a.Add(1); '
                   'DELEGATE p = (PARAM) => Gate.Read() == x; '
                   'try { int[] b = a.FindAll(p); delete[] b; } '
                   'catch (Exception e) { a.Add(2); return a.Count() == 2 ? 0 : 1; } return 2; } }')
    class_mutation = ('using NativeSharp.Core.Collections.Generic; public class Box { '
                      'public int Value; public Box(int x) { Value = x; } } '
                      'public class Tracker { public static List<Box> Target; '
                      'public static bool Mutate(PARAM) { Target.Clear(); return true; } } '
                      'public class Program { public static int Main() { Tracker.Target = new List<Box>(); '
                      'Tracker.Target.Add(new Box(1)); Tracker.Target.Add(new Box(2)); '
                      'DELEGATE p = (PARAM) => Tracker.Mutate(ARG); '
                      'Box[] a = Tracker.Target.FindAll(p); delete[] a; return 91; } }')
    scenarios = [('normal', normal, 0), ('mutation', mutation, 142),
                 ('throw-value', exceptional.replace('DELEGATE', 'Predicate<int>').replace('PARAM', 'int x'), 0),
                 ('throw-in', exceptional.replace('DELEGATE', 'InPredicate<int>').replace('PARAM', 'in int x'), 0),
                 ('class-value', class_mutation.replace('DELEGATE', 'Predicate<Box>').replace('PARAM', 'Box x').replace('ARG', 'x'), 142),
                 ('class-in', class_mutation.replace('DELEGATE', 'InPredicate<Box>').replace('PARAM', 'in Box x').replace('ARG', 'in x'), 142)]
    for name, source, expected in scenarios:
        (root / 'Main.ns').write_text(source, encoding='utf-8')
        for opt in ('-O0', '-O1', '-O2', '-O3'):
            output = root / (name + opt + '.exe')
            compile_project(consumer, output, '--emit-project-oop-exe', name + opt + '-cache', opt,
                            root / opt[1:])
            result = subprocess.run([str(output)], cwd=root, env=env, capture_output=True, timeout=15)
            if result.returncode != expected:
                raise RuntimeError(f'{name} {opt}: exit {result.returncode}, expected {expected}')
            print(f'PASS: binary SDK {name} {opt}', flush=True)


if __name__ == '__main__':
    main()
