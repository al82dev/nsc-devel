"""NSC-META-001: an NSMETA file is not rejected for the format label in its magic.

The writer spells the magic "NSMETA\\0\\0"; files from before the labels were removed carry
"NSMETA1\\0". The reader compared the eight bytes exactly, so such a file failed with "invalid
NativeSharp metadata magic" before any of its content was read. The label byte is now accepted
as a digit or zero and selects nothing; the header, hash and ABI fingerprint decide.

A small library is produced with the compiler under test. Its metadata must load as written and
with the label digit restored in the magic. Corrupting the "NSMETA" prefix, or putting a non-digit
in the label byte, must still be rejected as an invalid magic, so the check keeps rejecting what is
not NSMETA at all.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

PRODUCER = '''namespace MagicLabel
{
    public class Direct
    {
        public int Value;
        public Direct(int value) { Value = value; }
    }
}
'''


def run(args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=120)


def main():
    compiler = str(Path(sys.argv[1]).resolve())
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-nsmeta-magic-') as temporary:
        root = Path(temporary)
        producer = root / 'producer'
        producer.mkdir()
        (producer / 'Types.ns').write_text(PRODUCER, encoding='utf-8')
        (producer / 'Producer.nsproj').write_text(
            '<Project><PropertyGroup><Name>MagicLabelProducer</Name><OutputType>Library</OutputType>'
            '</PropertyGroup><ItemGroup><Compile Include="Types.ns"/></ItemGroup></Project>', encoding='utf-8')
        library = producer / 'Producer.lib'
        built = run([compiler, '--emit-project-oop-lib', str(producer / 'Producer.nsproj'), '-O0', '-o',
                     str(library), '--no-artifact-cache'], producer)
        metadata_path = library.with_suffix('.nsmeta')
        if built.returncode or not metadata_path.exists():
            print('FAIL producer', built.stdout + built.stderr, flush=True)
            return 1
        written = metadata_path.read_bytes()
        if written[:8] != b'NSMETA\x00\x00':
            failures.append(('writer magic', written[:8]))
        cases = {
            'magic as written': (written, None),
            'label digit in magic': (written[:6] + b'1' + written[7:], None),
            'damaged prefix': (b'XSMETA' + written[6:], 'invalid NativeSharp metadata magic'),
            'non-digit label': (written[:6] + b'X' + written[7:], 'invalid NativeSharp metadata magic'),
        }
        for index, (name, (content, expected)) in enumerate(cases.items()):
            folder = root / str(index)
            folder.mkdir()
            metadata = folder / 'Producer.nsmeta'
            metadata.write_bytes(content)
            (folder / 'Main.ns').write_text(
                'using MagicLabel; int Main() { Direct a = new Direct(1); return a.Value == 1 ? 0 : 1; }',
                encoding='utf-8')
            project = folder / 'Consumer.nsproj'
            project.write_text('<Project><PropertyGroup><Name>MagicLabel</Name></PropertyGroup>'
                               '<ItemGroup><Compile Include="Main.ns"/><LibraryReference Include="' +
                               library.as_posix() + '" Metadata="' + metadata.as_posix() +
                               '"/></ItemGroup></Project>', encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', str(project), '--no-banner'], folder)
            output = result.stdout + result.stderr
            print(name, 'exit', result.returncode, output.strip()[:200], flush=True)
            if expected is None and result.returncode != 0:
                failures.append((name, output))
            if expected is not None and (result.returncode == 0 or expected not in output):
                failures.append((name, output))
    if failures:
        print('FAIL', failures, flush=True)
        return 1
    print('nsmeta_magic_label: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
