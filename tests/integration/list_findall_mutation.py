"""Exercise mutation guards for both List.FindAll overloads."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--linker', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--case')
    args = parser.parse_args()
    root = args.work_dir.resolve()
    root.mkdir(parents=True, exist_ok=False)
    repo = Path(__file__).resolve().parent.parent.parent
    for library in ('NativeSharp.Core', 'NativeSharp.Core.Collections.Generic'):
        shutil.copytree(repo / 'lib' / library, root / 'lib' / library,
                        ignore=shutil.ignore_patterns('obj', 'bin'))
    probe = root / 'probe'
    probe.mkdir()
    project = probe / 'Probe.nsproj'
    project.write_text('<Project><PropertyGroup><Name>Probe</Name><OutputType>Exe</OutputType>'
                       '</PropertyGroup><ItemGroup><Compile Include="Main.ns"/>'
                       '<ProjectReference Include="../lib/NativeSharp.Core.Collections.Generic/NativeSharp.Core.Collections.Generic.nsproj"/>'
                       '</ItemGroup></Project>', encoding='utf-8')
    actions = {
        'clear': 'Target.Clear();',
        'remove': 'Target.RemoveAt(0);',
        'removevalue': 'Target.Remove(1);',
        'removeall': 'Predicate<int> q = (int x) => true; Target.RemoveAll(q);',
        'removeallin': 'InPredicate<int> q = (in int x) => true; Target.RemoveAll(q);',
        'add': 'Target.Add(7);',
        'insert': 'Target.Insert(0, 7);',
        'replace': 'Target.SetAt(0, 7);',
        'indexer': 'Target[0] = 7;',
        'reserve': 'Target.EnsureCapacity(100);',
        'trim': 'Target.TrimExcess();',
        'reverse': 'Target.Reverse();',
        'sort': 'Target.Sort();',
        'addrange': 'int[] a = new int[1]; Target.AddRange(a); delete[] a;',
        'insertrange': 'int[] a = new int[1]; Target.InsertRange(0, a); delete[] a;',
        'removerange': 'Target.RemoveRange(0, 1);',
        'refat': 'ref int p = ref Target.RefAt(0); p = 7;',
        'nested': 'Predicate<int> q = (int x) => true; int[] a = Target.FindAll(q); delete[] a; Target.Clear();',
    }
    env = os.environ.copy()
    env.update(TEMP=str(root), TMP=str(root), NSC_LINKER=str(args.linker.resolve()),
               NSC_WINDOWS_RUNTIME_LIBRARY=str(args.runtime.resolve()))
    for overload in ('value', 'in'):
        for name, action in actions.items():
            if args.case and name != args.case:
                continue
            param = 'int value' if overload == 'value' else 'in int value'
            delegate = 'Predicate<int>' if overload == 'value' else 'InPredicate<int>'
            source = ('using NativeSharp.Core.Collections.Generic; public class Tracker {'
                      ' public static List<int> Target; public static bool Invoke(' + param + ') { '
                      + action + ' return true; } } public class Program { public static int Main() {'
                      ' Tracker.Target = new List<int>(); Tracker.Target.Add(1); Tracker.Target.Add(2);'
                      ' ' + delegate + ' p = (' + param + ') => Tracker.Invoke(' +
                      ('value' if overload == 'value' else 'in value') + ');'
                      ' int[] output = Tracker.Target.FindAll(p); delete[] output; return 91; } }')
            (probe / 'Main.ns').write_text(source, encoding='utf-8')
            for opt in ('-O0', '-O2'):
                output = root / (overload + '-' + name + opt + '.exe')
                result = subprocess.run([str(args.compiler.resolve()), opt, '--no-artifact-cache',
                                         '--emit-project-oop-exe', str(project), '-o', str(output)],
                                        cwd=root, env=env, capture_output=True, text=True, timeout=60)
                if result.returncode:
                    raise RuntimeError(f'{overload} {name} {opt}: {result.stderr}')
                run = subprocess.run([str(output)], cwd=root, env=env, capture_output=True, timeout=15)
                if run.returncode != 142:
                    raise RuntimeError(f'{overload} {name} {opt}: exit {run.returncode}, expected 142')
                print(f'PASS: {overload} {name} {opt}', flush=True)


if __name__ == '__main__':
    main()
