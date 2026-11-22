"""Tanda 1: diagnosticos de la interoperabilidad C/C++ (items 1.1-1.10).

1.1 avisos de una sesion Clang correcta; 1.2 opcion -W y fix-its; 1.3 el aviso se imprime una sola vez;
1.4 campo C no importado explicado al usarlo; 1.5 tipo C en parametros no proyectados; 1.6 texto variadico;
1.7 diagnosticos del preprocesado auxiliar (sin disparador reproducible aislado: cubierto por las rutas de macros
existentes); 1.8 cadenas de include y de macros; 1.9 wrapper NSC no emitido (sin disparador reproducible: solo
cambia el texto); 1.10 linea del encabezado con subrayado. Los positivos ademas compilan y ejecutan en O0-O3,
directo, en biblioteca estatica y en DLL.
"""
import comun
from comun import Caso, positivo, diagnostico, fin

aviso = Caso('t1-aviso', '#warning aviso-de-prueba\nint g(void);\n', 'int g(void) { return 42; }\n')
diagnostico(aviso, 'return g();', ['aviso-de-prueba [-W#warnings]', '#warning aviso-de-prueba'], True)
positivo(aviso, 'return g();')

# 1.3: el aviso aparece una sola vez.
def una_vez():
    import re
    caso = Caso('t1-una-vez', '#warning unico-aviso\nint g(void);\n', 'int g(void) { return 42; }\n')
    base = comun.ROOT / caso.nombre
    base.mkdir(parents=True)
    refs = caso.reference(base) + comun.core(0)
    (base / 'Main.ns').write_text('int Main() { return g(); }\n', encoding='utf-8')
    comun.project(base, 'App', ['Main.ns'], refs)
    out = comun.nsc(base, 'App', 0, 'exe', 'app.exe', 'nsc').stdout
    if out.count('unico-aviso [-W') != 1:
        comun._fail('t1-una-vez', out)
    else:
        print('ok t1-una-vez', flush=True)
una_vez()

# EN: `_Float16` maps to Half since C3-01 (item 3.6), so its field is imported and the former rejection is a
#     positive read; the omitted-field diagnostic is kept with `_BitInt(100)`, which stays unmapped (C3-02).
#     `void*[N]` fields are imported since batch 4.
# ES: `_Float16` se mapea a Half desde C3-01 (item 3.6), asi su campo se importa y el rechazo anterior es una
#     lectura positiva; el diagnostico de campo omitido se conserva con `_BitInt(100)`, que sigue sin mapeo (C3-02).
#     Los campos `void*[N]` se importan desde la tanda 4.
campo = Caso('t1-campo-omitido', 'typedef struct S { int a; _Float16 pad; int b; } S;\nint g(S* s);\n',
             'int g(S* s) { return s->a + s->b; }\n')
positivo(campo, 'S s; s.a = 40; s.b = 2; s.pad = (NativeSharp.Core.Numerics.Half)0.0f; int p = (int)s.pad; return g(&s) + p;')
campo_bitint = Caso('t1-campo-omitido-bitint',
                    'typedef struct S { int a; _BitInt(100) pad; int b; } S;\nint g(S* s);\n',
                    'int g(S* s) { return s->a + s->b; }\n')
diagnostico(campo_bitint, 'S s; s.a = 40; s.b = 2; int p = (int)s.pad; return g(&s);',
            ["C/C++ field 'pad' of type '_BitInt(100)' was not imported"], False, opts=['0'])
positivo(campo_bitint, 'S s; s.a = 40; s.b = 2; return g(&s);')

param = Caso('t1-parametro', 'int g(typeof(nullptr) p);\n', None, standard='c23')
diagnostico(param, 'return g(null);', ["('typeof (nullptr)') is not yet mapped"], False, opts=['0'])

incl = Caso('t1-cadenas', '#include "sub.h"\nint g(void);\n', None)
sub = '#define BAD(x) ((x) + missing_thing)\n#define WRAP(x) BAD(x)\nstatic int f(void) { return WRAP(1); }\n'
diagnostico(incl, 'return 42;', ["use of undeclared identifier 'missing_thing'", "expanded from macro 'BAD'",
                                 "expanded from macro 'WRAP'", 'in file included from here',
                                 'static int f(void) { return WRAP(1); }'], False, extra={'sub.h': sub})
fixit = Caso('t1-fixit', 'struct P { int x; };\nstatic int f(struct P p) { return p.x }\n', None)
diagnostico(fixit, 'return 42;', ["fix-it: insert or replace with ';'"], False, opts=['0'])
fin()
