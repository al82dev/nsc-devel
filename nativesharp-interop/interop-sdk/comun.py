"""Common harness for the SDK interop batches (plan in A:\\trabajo-interoperabilidad\\implementacion-plan).
Arnes comun de las tandas de interoperabilidad del SDK.

A case is a C/C++ header, an optional C/C++ source compiled into a static library, and a NativeSharp body that
returns 42 on success. `positivo` runs the body in O0-O3:
  - directo: the body in `Main` of a project that references the header;
  - lib: the body in a public generic method of a static NativeSharp library; a separate consumer instantiates it
    without the header;
  - dll: the body in a public method of a NativeSharp DLL; a separate consumer calls it through the import library.
`rechazo` compiles the body in O0 and expects a failure whose output contains the given text.

Paths come from environment variables with local defaults:
  NSC, NSC_CLANG_ROOT, NSC_CORE_ROOT, NSC_LINKER, NSC_RUNTIME_LIB.
"""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
NSC = os.environ.get('NSC', str(REPO / 'build-integracion' / 'bin' / 'nsc.exe'))
CLANG_ROOT = os.environ.get('NSC_CLANG_ROOT', r'A:\al82dev-clang')
CORE_ROOT = os.environ.get('NSC_CORE_ROOT', str(REPO / 'build-integracion' / 'nativesharp-libraries-build'))
LINKER = os.environ.get('NSC_LINKER', r'A:\nslink-devel\bin\release\nslink.exe')
RUNTIME = os.environ.get('NSC_RUNTIME_LIB',
                         str(REPO / 'build-integracion' / 'generated' / 'NativeSharp.Runtime.Win32.lib'))
CLANG = str(Path(CLANG_ROOT) / 'bin' / 'clang.exe')
LLVM_LIB = str(Path(CLANG_ROOT) / 'bin' / 'llvm-lib.exe')
OPTS = [o for o in os.environ.get('NSC_OPTS', '0,1,2,3').split(',') if o]
MODES = [m for m in os.environ.get('NSC_MODES', 'directo,lib,dll').split(',') if m]
OBSERVE = '--observar' in sys.argv
ONLY = [a for a in sys.argv[1:] if not a.startswith('--')]

Path('A:/temp').mkdir(exist_ok=True)
ROOT = Path(tempfile.mkdtemp(prefix='interop-sdk-', dir='A:/temp'))
ENV = dict(os.environ, TEMP=str(ROOT), TMP=str(ROOT), NSC_LINKER=LINKER, NSC_LLVM_LIB=LLVM_LIB,
           NSC_WINDOWS_RUNTIME_LIBRARY=RUNTIME)
ENV['PATH'] = str(Path(NSC).parent) + os.pathsep + str(Path(CLANG_ROOT) / 'bin') + os.pathsep + \
    str(Path(LINKER).parent) + os.pathsep + ENV.get('PATH', '')
FAILURES = []
_COUNTERS = {}
print('Evidencia:', ROOT, flush=True)


def run(command, cwd, label=None):
    result = subprocess.run([str(c) for c in command], cwd=cwd, env=ENV, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, encoding='utf-8', errors='replace')
    if label:
        (Path(cwd) / (label + '.log')).write_text(f'COMMAND: {command!r}\nEXIT: {result.returncode}\n' +
                                                  result.stdout, encoding='utf-8')
    return result


def core(opt):
    folder = Path(CORE_ROOT) / f'O{opt}'
    return (f'<LibraryReference Include="{folder / "NativeSharpCore.lib"}" '
            f'Metadata="{folder / "NativeSharpCore.nsmeta"}" />')


class Caso:
    """header: text of api.h. source: C/C++ implementation (None = header only). language: C or C++."""

    def __init__(self, nombre, header, source=None, language='C', standard=None, extra_ref='', clang_args=(),
                 shared=False):
        self.nombre = nombre
        self.header = header
        self.source = source
        self.language = language
        self.standard = standard or ('c17' if language == 'C' else 'c++20')
        self.extra_ref = extra_ref
        self.clang_args = list(clang_args)
        # shared: the C/C++ source becomes capi.dll with its import library capi.lib (no CRT, no entry point).
        self.shared = shared

    def native(self, folder):
        folder.mkdir(parents=True, exist_ok=True)
        (folder / 'api.h').write_text(self.header, encoding='utf-8')
        if self.source is None:
            return ''
        ext = 'c' if self.language == 'C' else 'cpp'
        source = self.source if '#include "api.h"' in self.source else '#include "api.h"\n' + self.source
        (folder / f'api.{ext}').write_text(source, encoding='utf-8')
        std = f'-std={self.standard}'
        result = run([CLANG, '-c', '--target=x86_64-pc-windows-msvc', std, *self.clang_args, f'api.{ext}', '-o',
                      'api.obj'], folder, 'clang')
        if result.returncode:
            raise SystemExit(f'{self.nombre}: clang fallo:\n{result.stdout}')
        if self.shared:
            result = run([CLANG, '--target=x86_64-pc-windows-msvc', '-shared', '-fuse-ld=lld', '-nostdlib',
                          '-Wl,/noentry', 'api.obj', '-o', 'capi.dll', '-Wl,/implib:capi.lib'], folder, 'link-dll')
            if result.returncode:
                raise SystemExit(f'{self.nombre}: enlace de la DLL C fallo:\n{result.stdout}')
            return ' Library="capi.lib" Runtime="capi.dll"'
        result = run([LLVM_LIB, '/nologo', '/out:capi.lib', 'api.obj'], folder, 'llvm-lib')
        if result.returncode:
            raise SystemExit(f'{self.nombre}: llvm-lib fallo:\n{result.stdout}')
        return ' Library="capi.lib"'

    def reference(self, folder):
        library = self.native(folder)
        return (f'<CppLibraryReference Header="api.h" Language="{self.language}" Standard="{self.standard}"'
                f'{library}{self.extra_ref} />')


def project(folder, name, sources, refs, output_type=None):
    kind = f'<OutputType>{output_type}</OutputType>' if output_type else ''
    items = ''.join(f'<Compile Include="{s}" />' for s in sources)
    (folder / f'{name}.nsproj').write_text(f'<Project><PropertyGroup><Name>{name}</Name>{kind}</PropertyGroup>'
                                          f'<ItemGroup>{items}{refs}</ItemGroup></Project>', encoding='utf-8')


def nsc(folder, name, opt, kind, output, label):
    return run([NSC, f'--cpp-clang-root={CLANG_ROOT}', f'--emit-project-oop-{kind}', f'{name}.nsproj',
                '--no-artifact-cache', f'-O{opt}', '-o', output], folder, label)


def _fail(label, detail):
    FAILURES.append(label)
    print(f'FALLA {label}: {detail}', flush=True)


def _execute(label, exe):
    code = run([exe], exe.parent, label + '-run').returncode
    if code != 42:
        _fail(label, f'devolvio {code}, se esperaba 42')
        return False
    return True


def _selected(nombre):
    return not ONLY or any(o in nombre for o in ONLY)


def positivo(caso, body, preludio='', opts=None, modes=None):
    """body: statements of a method returning int (42 = success). preludio: NativeSharp declarations."""
    if not _selected(caso.nombre):
        return
    for opt in opts or OPTS:
        for mode in modes or MODES:
            label = f'{caso.nombre}-O{opt}-{mode}'
            base = ROOT / caso.nombre / f'O{opt}' / mode
            ok = _positivo_mode(caso, body, preludio, opt, mode, base, label)
            if ok:
                print(f'ok {label}', flush=True)


def _positivo_mode(caso, body, preludio, opt, mode, base, label):
    if mode == 'directo':
        base.mkdir(parents=True)
        refs = caso.reference(base) + core(opt)
        (base / 'Main.ns').write_text(f'using NativeSharp.Core;\n{preludio}\nint Main()\n{{\n{body}\n}}\n',
                                      encoding='utf-8')
        project(base, 'App', ['Main.ns'], refs)
        result = nsc(base, 'App', opt, 'exe', 'app.exe', 'nsc')
        if result.returncode:
            _fail(label, result.stdout[-1500:])
            return False
        return _execute(label, base / 'app.exe')
    producer = base / 'productor'
    consumer = base / 'consumidor'
    refs = caso.reference(producer) + core(opt)
    dll = mode == 'dll'
    signature = 'public static int Run()' if dll else 'public static int Run<T>(T unused)'
    (producer / 'Lib.ns').write_text(f'using NativeSharp.Core;\n{preludio}\nnamespace SdkProbe\n{{\n'
                                     f'    public class Probe\n    {{\n        {signature}\n        {{\n{body}\n'
                                     '        }\n    }\n}\n', encoding='utf-8')
    project(producer, 'SdkProbeLib', ['Lib.ns'], refs, 'SharedLibrary' if dll else 'Library')
    result = nsc(producer, 'SdkProbeLib', opt, 'dll' if dll else 'lib',
                 'SdkProbeLib.dll' if dll else 'SdkProbeLib.lib', 'nsc')
    if result.returncode:
        _fail(label + '-productor', result.stdout[-1500:])
        return False
    consumer.mkdir(parents=True)
    call = 'SdkProbe.Probe.Run()' if dll else 'SdkProbe.Probe.Run<int>(0)'
    (consumer / 'Main.ns').write_text(f'using NativeSharp.Core;\nint Main()\n{{\n    return {call};\n}}\n',
                                      encoding='utf-8')
    runtime = ' Runtime="../productor/SdkProbeLib.dll"' if dll else ''
    extra = ''
    project(consumer, 'App', ['Main.ns'],
            f'<LibraryReference Include="../productor/SdkProbeLib.lib"{runtime} />{extra}' + core(opt))
    result = nsc(consumer, 'App', opt, 'exe', 'app.exe', 'nsc')
    if result.returncode:
        _fail(label + '-consumidor', result.stdout[-1500:])
        return False
    if dll:
        (consumer / 'SdkProbeLib.dll').write_bytes((producer / 'SdkProbeLib.dll').read_bytes())
    return _execute(label, consumer / 'app.exe')


def rechazo(caso, body, texto, preludio=''):
    """Compiles in O0 and expects a failure whose output contains `texto`."""
    if not _selected(caso.nombre):
        return
    _COUNTERS[caso.nombre] = _COUNTERS.get(caso.nombre, 0) + 1
    label = f'{caso.nombre}-rechazo{_COUNTERS[caso.nombre]}'
    base = ROOT / caso.nombre / f'rechazo{_COUNTERS[caso.nombre]}'
    base.mkdir(parents=True)
    refs = caso.reference(base) + core(0)
    (base / 'Main.ns').write_text(f'using NativeSharp.Core;\n{preludio}\nint Main()\n{{\n{body}\n}}\n',
                                  encoding='utf-8')
    project(base, 'App', ['Main.ns'], refs)
    result = nsc(base, 'App', 0, 'exe', 'app.exe', 'nsc')
    if result.returncode == 0 or texto not in result.stdout:
        _fail(label, f'se esperaba un rechazo con "{texto}":\n{result.stdout[-1500:]}')
    else:
        print(f'ok {label}', flush=True)


def diagnostico(caso, body, textos, compila, preludio='', extra=None, opts=None):
    """Compiles the direct project in each optimization level and checks that the output contains every text.
    compila: whether the build must succeed. extra: additional files {name: text} next to api.h."""
    if not _selected(caso.nombre):
        return
    for opt in opts or OPTS:
        label = f'{caso.nombre}-O{opt}-diagnostico'
        base = ROOT / caso.nombre / f'O{opt}' / 'diagnostico'
        base.mkdir(parents=True)
        for name, text in (extra or {}).items():
            (base / name).write_text(text, encoding='utf-8')
        refs = caso.reference(base) + core(opt)
        (base / 'Main.ns').write_text(f'using NativeSharp.Core;\n{preludio}\nint Main()\n{{\n{body}\n}}\n',
                                      encoding='utf-8')
        project(base, 'App', ['Main.ns'], refs)
        result = nsc(base, 'App', opt, 'exe', 'app.exe', 'nsc')
        missing = [t for t in textos if t not in result.stdout]
        if (result.returncode == 0) != compila or missing:
            _fail(label, f'compila={result.returncode == 0} (esperado {compila}), faltan {missing}:\n'
                         f'{result.stdout[-1500:]}')
        else:
            print(f'ok {label}', flush=True)


def ir(caso, body, preludio='', opt=1):
    """Returns the optimized LLVM IR of the direct project (for comparisons against Clang)."""
    base = ROOT / caso.nombre / f'ir-O{opt}'
    base.mkdir(parents=True)
    refs = caso.reference(base) + core(opt)
    (base / 'Main.ns').write_text(f'using NativeSharp.Core;\n{preludio}\nint Main()\n{{\n{body}\n}}\n',
                                  encoding='utf-8')
    project(base, 'App', ['Main.ns'], refs)
    result = run([NSC, f'--cpp-clang-root={CLANG_ROOT}', '--dump-project-oop-llvm-opt', 'App.nsproj', f'-O{opt}'],
                 base, 'ir')
    return result


def fin():
    if FAILURES:
        print(f'FALLAS ({len(FAILURES)}): ' + ', '.join(FAILURES))
        print('Evidencia:', ROOT)
        raise SystemExit(1)
    print('OK; evidencia:', ROOT)
