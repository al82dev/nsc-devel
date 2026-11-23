"""Limitaciones R2: cache, catalogos y diagnosticos (plan de limitaciones, tareas R2-01..R2-09).

R2-03: con la cache de artefactos activa, cambiar solo un encabezado transitivo o solo un `Define` de la referencia
cambia el programa (no se reutiliza un artefacto viejo); se compara la semantica (valor devuelto), no la velocidad.
R2-05: `-fdebug-prefix-map` reescribe la ruta de origen en el PDB tambien para las unidades C del encabezado.
R2-08 (T17-005): una variable global C declarada y no definida se explica como una funcion: declaracion presente,
ninguna biblioteca la define.
R2-02 (equivalencia directo/precompilado) la cubren las fixtures limitaciones_c*/x* en sus modos .lib y DLL.
"""
import comun
from comun import Caso, rechazo, fin, _fail, ONLY


def cache_invalidation():
    base = comun.ROOT / 'r2-cache'
    base.mkdir(parents=True)
    caso = Caso('r2-cache', '#include "sub.h"\nint base_value(void);\n', 'int base_value(void) { return 30; }\n')
    (base / 'sub.h').write_text('#define K 12\n', encoding='utf-8')
    refs = caso.reference(base) + comun.core(0)
    (base / 'Main.ns').write_text('int Main() { return base_value() + K; }\n', encoding='utf-8')

    # The artifact cache is opt-in (`--artifact-cache=<dir>`); each step reports its bundle cache state.
    # La cache de artefactos se activa con `--artifact-cache=<dir>`; cada paso informa el estado de su cache.
    def build(define=None):
        reference = refs.replace(' />', f'><Define Value="{define}" /></CppLibraryReference>', 1) if define else refs
        comun.project(base, 'App', ['Main.ns'], reference)
        result = comun.run([comun.NSC, f'--cpp-clang-root={comun.CLANG_ROOT}', '--emit-project-oop-exe',
                            'App.nsproj', '-O0', '--verbose', f'--artifact-cache={base / "cache"}', '-o', 'app.exe'],
                           base, 'r2-cache')
        if result.returncode:
            return None, None
        state = next((line.split(':', 1)[1].strip() for line in result.stdout.splitlines()
                      if line.startswith('compilation-bundle-cache:')), '?')
        return comun.run([str(base / 'app.exe')], base, 'r2-cache-run').returncode, state

    steps = [build(), build()]
    (base / 'sub.h').write_text('#define K 11\n', encoding='utf-8')
    steps.append(build())
    (base / 'sub.h').write_text('#ifdef EXTRA\n#define K 13\n#else\n#define K 11\n#endif\n', encoding='utf-8')
    steps.append(build('EXTRA'))
    values = [value for value, _ in steps]
    hits = [state == 'hit' for _, state in steps]
    # Cold, warm (reused), changed transitive header (not reused), changed Define (not reused).
    if values != [42, 42, 41, 43] or hits != [False, True, False, False]:
        _fail('r2-cache', f'valores {values} y cache {[state for _, state in steps]}; se esperaba [42, 42, 41, 43] '
                          'con reutilizacion solo en el segundo paso')
    else:
        print('ok r2-cache', flush=True)


def prefix_map():
    base = comun.ROOT / 'r2-prefijo'
    base.mkdir(parents=True)
    caso = Caso('r2-prefijo', 'static inline int twice(int v) { return v * 2; }\nint base_value(void);\n',
                'int base_value(void) { return 21; }\n')
    refs = caso.reference(base) + comun.core(0)
    (base / 'Main.ns').write_text('int Main() { return twice(base_value()); }\n', encoding='utf-8')
    comun.project(base, 'App', ['Main.ns'], refs)
    result = comun.run([comun.NSC, f'-fdebug-prefix-map={base}=X:\\src', f'--cpp-clang-root={comun.CLANG_ROOT}',
                        '--emit-project-oop-exe', 'App.nsproj', '-O0', '-g', '--no-artifact-cache', '-o', 'app.exe'],
                       base, 'r2-prefijo')
    pdb = base / 'app.pdb'
    if result.returncode or not pdb.exists():
        _fail('r2-prefijo', result.stdout[-1500:])
        return
    data = pdb.read_bytes()
    if str(base).encode() in data or b'X:\\src' not in data:
        _fail('r2-prefijo', 'el PDB conserva la ruta original o no tiene la reescrita')
    else:
        print('ok r2-prefijo', flush=True)


if not ONLY or any('r2-cache' in o for o in ONLY):
    cache_invalidation()
if not ONLY or any('r2-prefijo' in o for o in ONLY):
    prefix_map()
rechazo(Caso('r2-global', 'extern int missing_global;\nint present(void);\n', 'int present(void) { return 1; }\n'),
        'return missing_global;', "C/C++ global variable 'missing_global' (symbol 'missing_global') is declared by "
        "the header but not defined by any linked library")
fin()
