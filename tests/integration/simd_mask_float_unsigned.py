"""Check floating masks converted to unsigned bit-pattern vectors."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile


def main():
    if os.name != 'nt' or len(sys.argv) != 2:
        raise SystemExit('usage: simd_mask_float_unsigned.py <Windows compiler>')
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-mask-float-unsigned-') as temporary:
        root = Path(temporary)
        for lane, bits, signed, one in (('float', 'uint', 'int', '1065353216'),
                                         ('double', 'ulong', 'long', '4607182418800017408')):
            folder = root / lane
            folder.mkdir()
            descriptor = folder / 'Probe.nsproj'
            descriptor.write_text('<Project><PropertyGroup><Name>MaskBits</Name></PropertyGroup>'
                                  '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
            body = '''class Program { public static int Main() {
                simd<LANE, 4> a = default(simd<LANE, 4>);
                simd<LANE, 4> b = default(simd<LANE, 4>);
                unsafe("inspect the resulting bit patterns") {
                    SIGNED* input = reinterpret_cast<SIGNED*>(&b);
                    input[1] = ONE; input[3] = ONE;
                    mask<LANE, 4> m = a == b;
                    simd<BITS, 4> v = (simd<BITS, 4>)m;
                    SIGNED* values = reinterpret_cast<SIGNED*>(&v);
                    return values[0] == -1 && values[1] == 0 &&
                           values[2] == -1 && values[3] == 0 ? 42 : 1;
                } } }'''.replace('LANE', lane).replace('SIGNED', signed).replace('BITS', bits).replace('ONE', one)
            (folder / 'Main.ns').write_text(body, encoding='utf-8')
            for level in ('O0', 'O2'):
                executable = folder / (level + '.exe')
                command = [str(compiler), '--emit-project-oop-exe', str(descriptor), '-o', str(executable),
                           '--no-artifact-cache', '-' + level]
                build = subprocess.run(command, cwd=root, capture_output=True, text=True, errors='replace', timeout=120)
                print('BUILD', lane, level, build.returncode, flush=True)
                if build.returncode != 0:
                    print(build.stdout + build.stderr, flush=True)
                    return 1
                run = subprocess.run([str(executable)], cwd=root, timeout=120)
                print('RUN', lane, level, run.returncode, flush=True)
                if run.returncode != 42:
                    return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
