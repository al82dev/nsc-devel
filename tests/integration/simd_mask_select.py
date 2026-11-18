"""Exercise lane-wise selection and scalar conditional behavior with real NSC."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


PROGRAM = '''class Program {
    public static int calls = 0;
    public static mask<int, 4> Cond(mask<int, 4> value) { calls = calls * 10 + 1; return value; }
    public static simd<int, 4> Left(simd<int, 4> value) { calls = calls * 10 + 2; return value; }
    public static simd<int, 4> Right(simd<int, 4> value) { calls = calls * 10 + 3; return value; }
    public static simd<int, 4> Value(int number) { return default(simd<int, 4>); }
    public static int Touch(int value) { calls = calls * 10 + value; return value; }
    public static int Main() {
        simd<int, 4> x = default(simd<int, 4>);
        simd<int, 4> y = default(simd<int, 4>);
        simd<int, 4> a = default(simd<int, 4>);
        simd<int, 4> b = default(simd<int, 4>);
        unsafe("prepare and inspect lanes in isolated test") {
            int* yp = reinterpret_cast<int*>(&y); yp[1] = 1; yp[3] = 1;
            int* ap = reinterpret_cast<int*>(&a); ap[0] = 10; ap[1] = 20; ap[2] = 30; ap[3] = 40;
            int* bp = reinterpret_cast<int*>(&b); bp[0] = 1; bp[1] = 2; bp[2] = 3; bp[3] = 4;
            mask<int, 4> m = x == y;
            simd<int, 4> result = Cond(m) ? Left(a) : Right(b);
            int* lanes = reinterpret_cast<int*>(&result);
            if (!(lanes[0] == 10 && lanes[1] == 2 && lanes[2] == 30 && lanes[3] == 4)) return 1;
            if (calls != 123) return 2;
            mask<int, 4> yes = x == x; mask<int, 4> no = x != x;
            mask<int, 4> chosen = m ? yes : no;
            if (!(chosen[0] && !chosen[1] && chosen[2] && !chosen[3])) return 3;
            calls = 0; bool flag = true;
            int scalar = flag ? Touch(7) : Touch(8);
            if (scalar != 7 || calls != 7) return 4;
            calls = 0; flag = false;
            scalar = flag ? Touch(7) : Touch(8);
            if (scalar != 8 || calls != 8) return 5;
            int initialized;
            simd<int, 4> flow = m ? Value(initialized = 1) : Value(initialized);
            if (initialized != 1) return 6;
            return 42;
        }
    }
}'''

FLOAT_PROGRAM = '''class Program { public static int Main() {
    simd<float, 4> x = default(simd<float, 4>); simd<float, 4> y = default(simd<float, 4>);
    simd<float, 4> a = default(simd<float, 4>); simd<float, 4> b = default(simd<float, 4>);
    unsafe("inspect float selection bits in isolated test") {
        int* yp = reinterpret_cast<int*>(&y); yp[1] = 1065353216; yp[3] = 1065353216;
        int* ap = reinterpret_cast<int*>(&a); ap[0] = 1065353216; ap[2] = 1073741824;
        int* bp = reinterpret_cast<int*>(&b); bp[1] = 1077936128; bp[3] = 1082130432;
        mask<float, 4> m = x == y; simd<float, 4> v = m ? a : b;
        int* lanes = reinterpret_cast<int*>(&v);
        if (lanes[0] == 1065353216 && lanes[1] == 1077936128 &&
            lanes[2] == 1073741824 && lanes[3] == 1082130432) return 42;
        return 1;
    }
} }'''


def invoke(command, directory):
    result = subprocess.run([str(part) for part in command], cwd=directory,
                            capture_output=True, text=True, errors='replace')
    output = result.stdout + result.stderr
    print('COMMAND', *command, 'EXIT', result.returncode, flush=True)
    if output:
        print(output, flush=True)
    return result


def project(root, name, body):
    directory = root / name
    directory.mkdir()
    descriptor = directory / 'Probe.nsproj'
    descriptor.write_text('<Project><PropertyGroup><Name>' + name + '</Name>'
                          '</PropertyGroup><ItemGroup><Compile Include="Main.ns"/>'
                          '</ItemGroup></Project>', encoding='utf-8')
    (directory / 'Main.ns').write_text(body, encoding='utf-8')
    return descriptor


def main():
    if os.name != 'nt' or len(sys.argv) != 2:
        raise SystemExit('usage: simd_mask_select.py <Windows compiler>')
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-89-mask-select-') as temporary:
        root = Path(temporary)
        for name, body in (('Mixed', PROGRAM), ('FloatMixed', FLOAT_PROGRAM)):
            descriptor = project(root, name, body)
            for level in ('O0', 'O2'):
                executable = descriptor.parent / (level + '.exe')
                built = invoke([compiler, '--emit-project-oop-exe', descriptor, '-o', executable,
                                '--no-artifact-cache', '-' + level], root)
                assert built.returncode == 0, built.stdout + built.stderr
                result = invoke([executable], root)
                assert result.returncode == 42, result.stdout + result.stderr
        for lane, count, input_type, one in (
                ('long', 2, 'long', '1'), ('int', 8, 'int', '1'),
                ('byte', 16, 'byte', '1'), ('double', 4, 'long', '4607182418800017408')):
            type_text = lane + ', ' + str(count)
            body = ('class Program { public static int Main() { '
                    'simd<' + type_text + '> a = default(simd<' + type_text + '>); '
                    'simd<' + type_text + '> b = default(simd<' + type_text + '>); '
                    'unsafe("prepare one lane") { ' + input_type + '* p = '
                    'reinterpret_cast<' + input_type + '*>(&b); p[1] = ' + one + '; } '
                    'mask<' + type_text + '> m = a == b; '
                    'simd<' + type_text + '> result = m ? a : b; '
                    'mask<' + type_text + '> same = result == b; int i = 0; '
                    'while (i < ' + str(count) + ') { if (!same[i]) return 1; i++; } '
                    'return 42; } }')
            descriptor = project(root, 'Count' + lane, body)
            for level in ('O0', 'O2'):
                executable = descriptor.parent / (level + '.exe')
                built = invoke([compiler, '--emit-project-oop-exe', descriptor, '-o', executable,
                                '--no-artifact-cache', '-' + level], root)
                assert built.returncode == 0, built.stdout + built.stderr
                result = invoke([executable], root)
                assert result.returncode == 42, result.stdout + result.stderr
        prefix = ('class Program { public static int Main() { '
                  'simd<int, 4> a = default(simd<int, 4>); '
                  'mask<int, 4> m = a == a; ')
        for name, statement in (
                ('WrongCount', 'simd<int, 2> b = default(simd<int, 2>); simd<int, 2> r = m ? b : b;'),
                ('WrongType', 'simd<long, 4> b = default(simd<long, 4>); simd<long, 4> r = m ? b : b;'),
                ('ScalarBranches', 'int r = m ? 1 : 2;')):
            descriptor = project(root, name, prefix + statement + ' return 42; } }')
            result = invoke([compiler, '--check-project-method-bodies', descriptor], root)
            assert result.returncode != 0 and 'SIMD mask selection requires matching' in result.stderr + result.stdout
        descriptor = project(root, 'ScalarFlow',
            'class Program { public static int Main() { int x; bool flag = true; '
            'int result = flag ? (x = 1) : 0; return x; } }')
        result = invoke([compiler, '--check-project-method-bodies', descriptor], root)
        assert result.returncode != 0 and 'assigned' in result.stderr + result.stdout
        print('simd_mask_select: PASS', flush=True)


if __name__ == '__main__':
    main()
