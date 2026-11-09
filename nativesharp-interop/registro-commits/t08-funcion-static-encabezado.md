# T08-003-04d1: función C `static` sin `inline` definida en un encabezado

Fecha: 2026-11-08. Casilla registrada en [t08-funciones-declaraciones](t08-funciones-declaraciones.md).

## Reproducción

Un encabezado C define `static int counter_x(void) { static int calls; return ++calls; }`. Usarla desde
NativeSharp daba «unknown identifier 'counter_x'». La misma función con `static inline` sí se importaba.

Causa: `ClangSession.cpp` solo admitía como definición C local las funciones con `static` **e** `inline`
(`isCStaticInlineDefinition` y la selección de cuerpos para exportar su IR). En C, una función `static`
definida en un encabezado da a cada unidad de traducción su propia copia, igual que `static inline`. El
`inline` es solo una sugerencia de optimización y no cambia el enlace.

## Cambio

`ClangSession.cpp`: una definición C con `static` y cuerpo se importa igual con o sin `inline`. El resto del
camino (símbolo propio por referencia, captura del cuerpo en la metadata y copia por productor) ya era común.
Una función `static` solo declarada, sin cuerpo, sigue sin importarse.

## Prueba

Nueva: `pruebas-tanda-08-funciones-c/validar-static-encabezado.py`. Resultado: aprobada.

- Control C con Clang: las mismas llamadas sobre los mismos encabezados.
- Directo, O0–O3, con dos encabezados:
  - cada `outer_*` llama a su propio `helper_*`;
  - cada contador conserva su variable local `static` (1, 2 y 101);
  - una declaración adelantada `static int forward_x(void);` va antes del cuerpo.
- Plantilla de un productor precompilado consumida sin encabezados, O0–O3.
- Plantillas de dos productores con `outer`/`helper` `static` homónimos y cuerpos distintos, O0–O3: cada
  una conserva el suyo (30040).
- Rechazos (O0):
  - dos encabezados del mismo proyecto definen `same()` y el proyecto la llama: «ambiguous namespace
    function call to 'same'»;
  - una función `static` declarada sin definición: «unknown identifier 'missing'».

Regresión dirigida (`claude/scripts/regresion-inline.cmd`):
- `pruebas-tanda-08-inline-c`, `inline-template` e `inline-thinlto-simbolo`;
- `identidad-c/inline-dos-bibliotecas` y `homonimos/validar-global-static.cmd`;
- `pruebas-tanda-08-prototipos-c`;
- `funciones-c/validar-declaraciones.py` en O0 y O3.
