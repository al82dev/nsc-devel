"""Compare receiver lifetime checking for source and precompiled libraries."""
import os
from pathlib import Path
import subprocess
import sys


compiler = Path(sys.argv[1])
work = Path(sys.argv[2])
producer = work / 'producer'
consumer = work / 'consumer'
producer.mkdir(parents=True)
consumer.mkdir()
(producer / 'Library.nsproj').write_text(
    '<Project><PropertyGroup><Name>Library</Name><OutputType>Library</OutputType>'
    '</PropertyGroup><ItemGroup><Compile Include="Library.ns" /></ItemGroup></Project>', encoding='utf-8')
(producer / 'Library.ns').write_text('''
public class Box { public void Touch() {} }
public class Holder {
    public Box saved;
    public void Save(Box value) { saved = value; }
    public void SaveViaHelper(Box value) { Save(value); }
    public void Read() { saved.Touch(); }
}
public class Keeper {
    public static Keeper Saved;
    public void Save() { Saved = this; }
    public void Touch() {}
}
''', encoding='utf-8')
cases = {
    'Safe': '''int Main() {
    for (int i = 0; i < 2; i = i + 1) { raii Box old = new Box(); old.Touch(); }
    raii Box current = new Box(); current.Touch(); return 0;
}''',
    'Field': '''int Main() {
    raii Holder holder = new Holder();
    { raii Box value = new Box(); holder.Save(value); }
    holder.Read(); return 0;
}''',
    'Helper': '''int Main() {
    raii Holder holder = new Holder();
    { raii Box value = new Box(); holder.SaveViaHelper(value); }
    holder.Read(); return 0;
}''',
    'Escape': '''int Main() {
    { raii Keeper owner = new Keeper(); owner.Save(); }
    Keeper.Saved.Touch(); return 0;
}''',
}
references = {
    'Source': '<ProjectReference Include="../producer/Library.nsproj" />',
    'Binary': '<LibraryReference Include="../producer/Library.lib" Metadata="../producer/Library.nsmeta" />',
}
for name, code in cases.items():
    (consumer / (name + '.ns')).write_text(code, encoding='utf-8')
    for mode, reference in references.items():
        (consumer / (name + mode + '.nsproj')).write_text(
            '<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup><ItemGroup><Compile Include="' +
            name + '.ns" />' + reference + '</ItemGroup></Project>', encoding='utf-8')

env = os.environ.copy()
env.pop('NS_LIB_ROOT', None)
for level in ('O0', 'O1', 'O2', 'O3'):
    command = [str(compiler), '--emit-project-oop-lib', str(producer / 'Library.nsproj'), '-' + level,
               '-o', str(producer / 'Library.lib'), '--no-artifact-cache']
    result = subprocess.run(command, cwd=work, env=env, capture_output=True, text=True, timeout=120)
    if result.returncode:
        raise RuntimeError((command, result.stdout, result.stderr))
    for name in cases:
        for mode in references:
            exe = consumer / (name + mode + '.exe')
            command = [str(compiler), '--emit-project-oop-exe', str(consumer / (name + mode + '.nsproj')),
                       '-' + level, '-o', str(exe), '--no-artifact-cache']
            result = subprocess.run(command, cwd=work, env=env, capture_output=True, text=True, timeout=120)
            output = result.stdout + result.stderr
            if name == 'Safe':
                if result.returncode:
                    raise RuntimeError((command, output))
                run = subprocess.run([str(exe)], cwd=work, env=env, capture_output=True, text=True, timeout=30)
                if run.returncode:
                    raise RuntimeError((exe, run.returncode, run.stdout, run.stderr))
            elif result.returncode == 0 or 'object' not in output or 'error:' not in output:
                raise RuntimeError((command, result.returncode, output))
    print(level + ': source and binary lifetime checks passed', flush=True)
