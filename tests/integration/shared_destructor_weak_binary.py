"""Exercise exceptional payload destruction through the public binary Core."""
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
    failures = []
    def hashes():
        return {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in (compiler, core, core.with_suffix('.nsmeta'))}
    before = hashes()
    print('SHA256 BEFORE', before, flush=True)
    with tempfile.TemporaryDirectory(prefix='nsc-binary-weak-destructor-', dir=temp_name) as temporary:
        root = Path(temporary)
        env = os.environ.copy()
        for key in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'): env[key] = str(root)

        def invoke(command):
            command = [str(item) for item in command]
            result = subprocess.run(command, cwd=repo, env=env, capture_output=True,
                                    text=True, errors='replace', timeout=120)
            print('CWD', repo, 'COMMAND', subprocess.list2cmdline(command), 'EXIT',
                  result.returncode, result.stdout + result.stderr, flush=True)
            return result.returncode

        def attr(path): return escape(path.as_posix(), {'"': '&quot;'})
        template = '''using NativeSharp.Core;
class Trace { public static int Drops; }
class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 1; } }
KIND Payload { public Payload() { } ~Payload() { Trace.Drops = Trace.Drops + 1; int ignored = Gate.Read(); } }
class Program { public static int Main() {
 WEAK<Payload> weak; int caught = 0;
 try { STRONG<Payload> owner = SharedPointers.MakeSTRONG<Payload>(); weak = owner.Downgrade(); }
 catch (Exception error) { caught = caught + 1; }
 if (caught != 1 || Trace.Drops != 1 || !weak.Expired() || weak.WeakCount() != 1l) return 1;
 STRONG<Payload> restored = weak.Upgrade(); if (!restored.IsEmpty()) return 2;
 return 42;
} }'''
        for family in ('Rc', 'Arc'):
            for kind in ('class', 'struct'):
                folder = root / (family + '-' + kind)
                folder.mkdir()
                source = template.replace('WEAK', 'Weak' + family).replace('STRONG', family).replace('KIND', kind)
                (folder / 'Main.ns').write_text(source, encoding='utf-8')
                project = folder / 'Main.nsproj'
                project.write_text('<Project><PropertyGroup><Name>WeakCleanupProbe</Name></PropertyGroup><ItemGroup>'
                                   '<Compile Include="Main.ns"/><LibraryReference Include="' + attr(core) +
                                   '" Metadata="' + attr(core.with_suffix('.nsmeta')) + '"/></ItemGroup></Project>', encoding='utf-8')
                for level in ('O0', 'O2'):
                    label = (family, kind, level)
                    executable = folder / (level + '.exe')
                    if invoke([compiler, '--emit-project-oop-exe', project, '-' + level,
                               '-o', executable, '--no-artifact-cache']):
                        failures.append(label + ('build',))
                    elif invoke([executable]) != 42:
                        failures.append(label + ('run',))
    after = hashes()
    print('SHA256 AFTER', after, flush=True)
    if before != after: failures.append(('input-hash-changed',))
    if failures: print('FAIL', failures, file=sys.stderr, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
