"""Verify the complete supported SIMD mask shape matrix with the real compiler."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


SHAPES = (
    ('sbyte', 'sbyte', 'sbyte', '1', (2, 4, 8, 16, 32, 64)),
    ('byte', 'byte', 'sbyte', '1', (2, 4, 8, 16, 32, 64)),
    ('short', 'short', 'short', '1', (2, 4, 8, 16, 32)),
    ('ushort', 'ushort', 'short', '1', (2, 4, 8, 16, 32)),
    ('int', 'int', 'int', '1', (2, 4, 8, 16)),
    ('uint', 'uint', 'int', '1', (2, 4, 8, 16)),
    ('float', 'int', 'int', '1065353216', (2, 4, 8, 16)),
    ('long', 'long', 'long', '1', (2, 4, 8)),
    ('ulong', 'ulong', 'long', '1', (2, 4, 8)),
    ('double', 'long', 'long', '4607182418800017408', (2, 4, 8)),
)


def invoke(command, directory, expected_zero=True):
    command = [str(part) for part in command]
    result = subprocess.run(command, cwd=directory, capture_output=True, text=True,
                            errors='replace', timeout=180)
    if expected_zero is not None and (result.returncode == 0) != expected_zero:
        print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
              file=sys.stderr, flush=True)
        print(result.stdout + result.stderr, file=sys.stderr, flush=True)
        raise AssertionError('unexpected command result')
    return result


def write_project(directory, name, body, output_type=None):
    directory.mkdir(parents=True, exist_ok=True)
    properties = '<Name>' + name + '</Name>'
    if output_type is not None:
        properties += '<OutputType>' + output_type + '</OutputType>'
    project = directory / (name + '.nsproj')
    project.write_text('<Project><PropertyGroup>' + properties + '</PropertyGroup>'
                       '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>',
                       encoding='utf-8')
    (directory / 'Main.ns').write_text(body, encoding='utf-8')
    return project


def shape_program(lane, storage, signed, one, count):
    replacements = {
        'LANE': lane, 'STORAGE': storage, 'SIGNED': signed,
        'ONE': one, 'COUNT': str(count),
    }
    body = '''class Program { public static int Main() {
        simd<LANE, COUNT> a = default(simd<LANE, COUNT>);
        simd<LANE, COUNT> b = default(simd<LANE, COUNT>);
        unsafe("prepare and inspect every SIMD lane") {
            STORAGE* input = reinterpret_cast<STORAGE*>(&b);
            int fill = 1;
            while (fill < COUNT) { input[fill] = ONE; fill = fill + 2; }
            mask<LANE, COUNT> m = a == b;
            simd<LANE, COUNT> bits = (simd<LANE, COUNT>)m;
            SIGNED* rawBits = reinterpret_cast<SIGNED*>(&bits);
            simd<LANE, COUNT> chosen = m ? a : b;
            mask<LANE, COUNT> chosenEqualsB = chosen == b;
            int i = 0;
            while (i < COUNT) {
                bool expected = (i & 1) == 0;
                if (m[i] != expected) return 1;
                if (rawBits[i] != (expected ? -1 : 0)) return 2;
                if (!chosenEqualsB[i]) return 3;
                i++;
            }
            return 42;
        }
    } }'''
    for old, new in replacements.items():
        body = body.replace(old, new)
    return body


def check_valid_shapes(compiler, root):
    total = 0
    for lane, storage, signed, one, counts in SHAPES:
        for count in counts:
            directory = root / 'shapes' / (lane + str(count))
            project = write_project(directory, 'Shape' + lane + str(count),
                                    shape_program(lane, storage, signed, one, count))
            for level in ('O0', 'O2'):
                executable = directory / (level + '.exe')
                invoke([compiler, '--emit-project-oop-exe', project, '-' + level,
                        '-o', executable, '--no-artifact-cache'], root)
                result = invoke([executable], root, expected_zero=False)
                if result.returncode != 42:
                    raise AssertionError(lane + str(count) + ' ' + level +
                                         ' returned ' + str(result.returncode))
                total += 1
    print('VALID_SHAPES', total, 'PASS', flush=True)


def check_invalid_shapes(compiler, root):
    shapes = ('mask<int, 1>', 'mask<int, 3>', 'mask<double, 16>',
              'mask<bool, 4>', 'mask<decimal, 4>')
    total = 0
    for ordinal, shape in enumerate(shapes):
        directory = root / 'invalid-shapes' / str(ordinal)
        body = ('class Program { public static int Main() { ' + shape +
                ' value = default(' + shape + '); return 0; } }')
        project = write_project(directory, 'InvalidShape' + str(ordinal), body)
        for level in ('O0', 'O2'):
            invoke([compiler, '--check-project-method-bodies', project, '-' + level],
                   root, expected_zero=False)
            total += 1
    print('INVALID_SHAPES', total, 'PASS', flush=True)


def check_index_types(compiler, root):
    valid_types = ('sbyte', 'byte', 'char', 'short', 'ushort', 'int', 'uint',
                   'long', 'ulong', 'nint', 'nuint', 'i128', 'u128')
    declarations = []
    conditions = []
    for ordinal, type_name in enumerate(valid_types):
        declarations.append(type_name + ' index' + str(ordinal) + ' = (' + type_name + ')0;')
        conditions.append('m[index' + str(ordinal) + ']')
    body = ('class Program { public static int Main() { simd<int, 4> a = '
            'default(simd<int, 4>); mask<int, 4> m = a == a; ' +
            ' '.join(declarations) + ' if (' + ' && '.join(conditions) +
            ') return 42; return 1; } }')
    directory = root / 'index-types' / 'valid'
    project = write_project(directory, 'ValidIndexTypes', body)
    for level in ('O0', 'O2'):
        executable = directory / (level + '.exe')
        invoke([compiler, '--emit-project-oop-exe', project, '-' + level,
                '-o', executable, '--no-artifact-cache'], root)
        result = invoke([executable], root, expected_zero=False)
        if result.returncode != 42:
            raise AssertionError('valid index types failed at ' + level)

    invalid = (('bool', 'true'), ('float', '0.0f'), ('double', '0.0'),
               ('decimal', '0.0m'))
    for ordinal, (type_name, literal) in enumerate(invalid):
        directory = root / 'index-types' / ('invalid' + str(ordinal))
        body = ('class Program { public static int Main() { simd<int, 4> a = '
                'default(simd<int, 4>); mask<int, 4> m = a == a; ' + type_name +
                ' i = ' + literal + '; bool value = m[i]; return value ? 0 : 1; } }')
        project = write_project(directory, 'InvalidIndex' + str(ordinal), body)
        for level in ('O0', 'O2'):
            result = invoke([compiler, '--check-project-method-bodies', project,
                             '-' + level], root, expected_zero=False)
            if 'SIMD mask indexing requires one integral index' not in result.stdout + result.stderr:
                raise AssertionError('missing invalid-index diagnostic for ' + type_name)
    print('INDEX_TYPES 13 valid, 4 invalid: PASS', flush=True)


def check_invalid_conversions(compiler, root):
    cases = (
        ('IntToUint', 'mask<int, 4> m = a == a; simd<uint, 4> v = (simd<uint, 4>)m;',
         'simd<int, 4> a = default(simd<int, 4>);'),
        ('DoubleToLong', 'mask<double, 4> m = a == a; simd<long, 4> v = (simd<long, 4>)m;',
         'simd<double, 4> a = default(simd<double, 4>);'),
        ('FloatToUlong', 'mask<float, 4> m = a == a; simd<ulong, 4> v = (simd<ulong, 4>)m;',
         'simd<float, 4> a = default(simd<float, 4>);'),
        ('Reverse', 'mask<int, 4> m = (mask<int, 4>)a;',
         'simd<int, 4> a = default(simd<int, 4>);'),
    )
    total = 0
    for name, statement, setup in cases:
        directory = root / 'invalid-conversions' / name
        project = write_project(directory, name,
            'class Program { public static int Main() { ' + setup + statement +
            ' return 0; } }')
        for level in ('O0', 'O2'):
            executable = directory / (level + '.exe')
            invoke([compiler, '--emit-project-oop-exe', project, '-' + level,
                    '-o', executable, '--no-artifact-cache'], root, expected_zero=False)
            total += 1
    print('INVALID_CONVERSIONS', total, 'PASS', flush=True)


def check_binary_boundary(compiler, root, expect_gap):
    producer_body = '''public class MaskApi {
        public static mask<int, 4> CompareInt(simd<int, 4> a, simd<int, 4> b) { return a == b; }
        public static bool ReadInt(mask<int, 4> value, int index) { return value[index]; }
        public static simd<int, 4> ChooseInt(mask<int, 4> value, simd<int, 4> yes,
                                              simd<int, 4> no) { return value ? yes : no; }
        public static mask<float, 8> CompareFloat(simd<float, 8> a, simd<float, 8> b) {
            return a == b;
        }
        public static simd<uint, 8> BitsFloat(mask<float, 8> value) {
            return (simd<uint, 8>)value;
        }
    }'''
    consumer_body = '''class Program { public static int Main() {
        simd<int, 4> a = default(simd<int, 4>); simd<int, 4> b = default(simd<int, 4>);
        unsafe("prepare and inspect cross-module masks") {
            int* bp = reinterpret_cast<int*>(&b); bp[1] = 1; bp[3] = 1;
            mask<int, 4> m = MaskApi.CompareInt(a, b);
            if (!MaskApi.ReadInt(m, 0) || MaskApi.ReadInt(m, 1) ||
                !MaskApi.ReadInt(m, 2) || MaskApi.ReadInt(m, 3)) return 1;
            simd<int, 4> chosen = MaskApi.ChooseInt(m, a, b);
            int* lanes = reinterpret_cast<int*>(&chosen);
            if (lanes[0] != 0 || lanes[1] != 1 || lanes[2] != 0 || lanes[3] != 1) return 2;
            simd<float, 8> f = default(simd<float, 8>);
            simd<uint, 8> bits = MaskApi.BitsFloat(MaskApi.CompareFloat(f, f));
            int* raw = reinterpret_cast<int*>(&bits); int i = 0;
            while (i < 8) { if (raw[i] != -1) return 3; i++; }
            return 42;
        }
    } }'''
    pairs = (('O0', 'O2'), ('O2', 'O0'))
    gaps = 0
    for producer_level, consumer_level in pairs:
        pair = root / 'binary' / (producer_level + '-' + consumer_level)
        producer = pair / 'producer'
        consumer = pair / 'consumer'
        producer_project = write_project(producer, 'MaskProducer', producer_body, 'Library')
        library = producer / 'MaskProducer.lib'
        invoke([compiler, '--emit-project-oop-lib', producer_project, '-' + producer_level,
                '-o', library, '--no-artifact-cache'], pair)
        metadata = library.with_suffix('.nsmeta')
        if not library.exists() or not metadata.exists():
            raise AssertionError('producer did not emit library and metadata')
        invoke([compiler, '--verify-library-metadata', metadata], pair)
        abi = invoke([compiler, '--dump-library-abi', metadata], pair)
        for method in ('CompareInt', 'ReadInt', 'ChooseInt', 'CompareFloat', 'BitsFloat'):
            if method not in abi.stdout + abi.stderr:
                raise AssertionError('ABI report omitted ' + method)
        (producer / 'Main.ns').unlink()
        consumer.mkdir(parents=True, exist_ok=True)
        consumer_project = consumer / 'MaskConsumer.nsproj'
        consumer_project.write_text(
            '<Project><PropertyGroup><Name>MaskConsumer</Name></PropertyGroup><ItemGroup>'
            '<Compile Include="Main.ns"/><LibraryReference Include="' + library.as_posix() +
            '" Metadata="' + metadata.as_posix() + '"/></ItemGroup></Project>', encoding='utf-8')
        (consumer / 'Main.ns').write_text(consumer_body, encoding='utf-8')
        executable = consumer / 'MaskConsumer.exe'
        built = invoke([compiler, '--emit-project-oop-exe', consumer_project, '-' + consumer_level,
                        '-o', executable, '--no-artifact-cache'], pair, expected_zero=None)
        if built.returncode != 0:
            output = built.stdout + built.stderr
            for method in ('CompareInt', 'ReadInt', 'ChooseInt', 'CompareFloat', 'BitsFloat'):
                if "no compatible member named '" + method + "'" not in output:
                    raise AssertionError('binary gap changed for ' + method + '\n' + output)
            if not expect_gap:
                print(output, file=sys.stderr, flush=True)
                raise AssertionError('binary SIMD mask signatures are not consumable')
            gaps += 1
            continue
        if expect_gap:
            raise AssertionError('expected binary gap was not reproduced')
        result = invoke([executable], pair, expected_zero=False)
        if result.returncode != 42:
            raise AssertionError('binary consumer returned ' + str(result.returncode))
    if gaps:
        print('BINARY_GAP', gaps, 'producer/consumer pairs CONFIRMED', flush=True)
    else:
        print('BINARY O0-O2 and O2-O0: PASS', flush=True)


def main():
    if os.name != 'nt' or len(sys.argv) not in (2, 3):
        raise SystemExit('usage: simd_mask_complete_matrix.py <Windows compiler> [--expect-binary-gap]')
    expect_binary_gap = len(sys.argv) == 3 and sys.argv[2] == '--expect-binary-gap'
    if len(sys.argv) == 3 and not expect_binary_gap:
        raise SystemExit('unknown option: ' + sys.argv[2])
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-89-mask-complete-') as temporary:
        root = Path(temporary)
        check_valid_shapes(compiler, root)
        check_invalid_shapes(compiler, root)
        check_index_types(compiler, root)
        check_invalid_conversions(compiler, root)
        check_binary_boundary(compiler, root, expect_binary_gap)
    print('simd_mask_complete_matrix: ' +
          ('PASS_WITH_EXPECTED_BINARY_GAP' if expect_binary_gap else 'PASS'), flush=True)


if __name__ == '__main__':
    main()
