"""Clang controls for the batch 19 audit probes."""
import subprocess, tempfile
from pathlib import Path
root = Path(tempfile.mkdtemp(prefix='nsc-t19-clang-', dir='A:/temp'))
SYS = ['-isystem', 'A:/vc2026-runtime/toolsets/14.52.36815/include', '-isystem', 'A:/vc2026-runtime/windows-sdk/10.0.28000.0/include/ucrt']
LIBS = ['A:/vc2026-runtime/toolsets/14.52.36815/lib/x64/libcmt.lib', 'A:/vc2026-runtime/toolsets/14.52.36815/lib/x64/libvcruntime.lib',
        'A:/vc2026-runtime/toolsets/14.52.36815/lib/x64/libcpmt.lib', 'A:/vc2026-runtime/windows-sdk/10.0.28000.0/lib/ucrt/x64/libucrt.lib',
        'A:/vc2026-runtime/windows-sdk/10.0.28000.0/lib/um/x64/kernel32.lib']
G = 'int g(int) { return 1; } int g(long) { return 2; }\n'
U = 'int g(unsigned) { return 1; } int g(unsigned long) { return 2; }\n'
L = 'int g(long) { return 1; } int g(long long) { return 2; }\n'
CASES = {
    'intlong': G + 'int main() { return g(5); }',
    'shortlong': G + 'int main() { short s = 5; return g(s); }',
    'uint': U + 'int main() { unsigned u = 5; return g(u); }',
    'longlong': L + 'int main() { long long v = 5; return g(v); }',
    'amb-int': L + 'int main() { return g(5); }',
    'pick': 'const int& pick(const int& a, const int& b) { return a < b ? b : a; }\nint main() { int a = 3, b = 9; return pick(a, b); }',
    'union': 'union U { int i; float f; };\nint main() { U u; u.i = 9; return u.i; }',
    'index': 'struct V { int x; int operator[](int i) const { return x + i; } };\nint main() { V v; v.x = 1; return v[2]; }',
    'vbase': 'struct B { int b = 4; }; struct D : virtual B { int d = 1; };\nint main() { D d; return d.b; }',
    'vartemplate': 'template <class T> constexpr T cero = T(7);\nint main() { return cero<int>; }',
    'pair': '#include <utility>\nint main() { std::pair<int, int> p(3, 4); return p.first + p.second; }',
    'private': 'class K { int secret = 1; public: int open = 2; };\nint main() { K k; return k.secret; }',
}
for name, src in CASES.items():
    (root / (name + '.cpp')).write_text(src + '\n', encoding='utf-8')
    r = subprocess.run(['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', *SYS, name + '.cpp', '-c', '-o', name + '.obj'],
                       cwd=root, capture_output=True, text=True)
    if r.returncode:
        err = [l for l in r.stderr.splitlines() if 'error' in l]
        print(f'{name}: Clang rechaza: {err[0] if err else r.stderr[:200]}')
        continue
    subprocess.run(['lld-link.exe', '/nologo', '/subsystem:console', '/out:' + name + '.exe', name + '.obj', *LIBS], cwd=root, check=True)
    print(f'{name}: {subprocess.run([str(root / (name + ".exe"))]).returncode}')
print('Evidencia:', root)
