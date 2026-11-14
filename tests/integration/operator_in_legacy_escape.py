"""Reject owning operator temporaries against a legacy producer that leaks their fields.

The archived producer is now refused as undecodable metadata; see the comment in main()."""
from pathlib import Path
import hashlib
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1]).resolve()
    library = (Path(sys.argv[2]) if len(sys.argv) > 2 else
               Path(__file__).resolve().parent / 'operator_in_legacy_escape_fixture' / 'Producer.lib').resolve()
    metadata = library.with_suffix('.nsmeta')
    expected = {
        library: 'e0c89507924728b9524647b518c62e11733951b960336ce97e19dd887b8ed836',
        metadata: '7122789f4d72bad33a225549c9ee166f3178a821d59533a3a3ead1a92c7619c0',
    }
    for path, digest in expected.items():
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise AssertionError('legacy producer fixture changed: ' + str(path))
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-legacy-operator-escape-') as temporary:
        root = Path(temporary)
        project = root / 'Consumer.nsproj'
        project.write_text('<Project><PropertyGroup><Name>LegacyEscape</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/><LibraryReference Include="' +
                           library.as_posix() + '" Metadata="' + metadata.as_posix() +
                           '"/></ItemGroup></Project>', encoding='utf-8')
        for kind in ('Direct', 'Alias', 'ViaCall'):
            for temporary_operands in (False, True):
                body = ('return new {0}(1) + new {0}(2);' if temporary_operands else
                        '{0} a = new {0}(1); {0} b = new {0}(2); return a + b;').format(kind)
                (root / 'Main.ns').write_text('int Main() { ' + body + ' }', encoding='utf-8')
                command = [str(compiler), '--check-project-method-bodies', str(project), '--no-banner']
                result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                        errors='replace', timeout=120)
                output = result.stdout + result.stderr
                print(kind, 'temporaries' if temporary_operands else 'locals',
                      'COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                      output, flush=True)
                # NSC-META-001 / NSC-NEW-010: the archived records have an older shape that cannot be
                # told apart from the current structure by content, and no label is used to choose a
                # reading. The archive must therefore never be imported: every consumer is refused
                # with a structural diagnostic, never by magic or version, and never accepted. The
                # escape barrier itself (temporaries refused without non-escape evidence, locals and
                # proven operators accepted) is checked on current content by
                # nsc.compiler.operator-in-missing-evidence.
                lowered = output.lower()
                structural = ('cannot import precompiled metadata' in lowered and
                              ('invalid metadata' in lowered or 'truncated' in lowered))
                by_label = any(word in lowered for word in ('magic', 'version', 'format minor'))
                if result.returncode == 0:
                    failures.append((kind, 'archived metadata imported'))
                elif not structural or by_label:
                    failures.append((kind, 'missing structural diagnostic'))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('operator_in_legacy_escape: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
