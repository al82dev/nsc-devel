"""Check direct receiver publication at emission, without executing programs."""
from pathlib import Path
from xml.sax.saxutils import escape
import hashlib
import os
import subprocess
import sys
import tempfile


def main():
    repo = Path(__file__).resolve().parents[2]
    compiler = Path(sys.argv[1]).resolve()
    core_name = os.environ.get('NSC_TEST_CORE_BINARY')
    temp_name = os.environ.get('NSC_TEST_TEMP_ROOT') or os.environ.get('TEMP')
    if not core_name or not Path(core_name).is_file() or not Path(core_name).with_suffix('.nsmeta').is_file():
        print('SKIP: NSC_TEST_CORE_BINARY must name an isolated Core library with metadata')
        return 77
    if not temp_name or Path(temp_name).drive.upper() != 'A:' or not Path(temp_name).is_dir():
        print('SKIP: NSC_TEST_TEMP_ROOT or TEMP must name an existing directory on A:')
        return 77
    core = Path(core_name).resolve()
    def hashes():
        return {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in (compiler, core, core.with_suffix('.nsmeta'))}
    before = hashes()
    print('SHA256 BEFORE', before, flush=True)
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-receiver-publication-', dir=temp_name) as temporary:
        root = Path(temporary)
        env = os.environ.copy()
        for key in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'): env[key] = str(root)
        def attr(path): return escape(path.as_posix(), {'"': '&quot;'})
        for family in ('Rc', 'Arc'):
            for case in ('constructor', 'destructor', 'control'):
                folder = root / (family + '-' + case)
                folder.mkdir()
                constructor = 'public Payload() { Alias = this; }' if case == 'constructor' else 'public Payload() { }'
                destructor = '~Payload() { Alias = this; }' if case == 'destructor' else ''
                source = ('using NativeSharp.Core; class Payload { public static Payload Alias; public int Data; ' +
                          constructor + destructor + ' } class Program { public static int Main() { ' +
                          family + '<Payload> owner = SharedPointers.Make' + family + '<Payload>(); return 0; } }')
                (folder / 'Main.ns').write_text(source, encoding='utf-8')
                project = folder / 'Probe.nsproj'
                project.write_text('<Project><PropertyGroup><Name>ReceiverPublication</Name></PropertyGroup><ItemGroup>'
                                   '<Compile Include="Main.ns"/><LibraryReference Include="' + attr(core) +
                                   '" Metadata="' + attr(core.with_suffix('.nsmeta')) + '"/></ItemGroup></Project>', encoding='utf-8')
                command = [str(compiler), '--emit-project-oop-exe', str(project), '-O0',
                           '-o', str(folder / 'probe.exe'), '--no-artifact-cache']
                result = subprocess.run(command, cwd=repo, env=env, capture_output=True,
                                        text=True, errors='replace', timeout=120)
                output = result.stdout + result.stderr
                print('CWD', repo, 'COMMAND', subprocess.list2cmdline(command),
                      'EXIT', result.returncode, output, flush=True)
                if case == 'control':
                    if result.returncode != 0: failures.append((family, case, 'unexpected-rejection'))
                elif result.returncode == 0 or 'shared payload publishes its receiver during construction or destruction' not in output:
                    failures.append((family, case, 'wrong-rejection'))
    after = hashes()
    print('SHA256 AFTER', after, flush=True)
    if before != after: failures.append(('input-hash-changed',))
    if failures: print('FAIL', failures, file=sys.stderr, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
