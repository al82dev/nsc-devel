# T08-016: arreglos fijos C

Fecha: 2026-11-08. Casillas: T08-016-03a–e y T08-016-04a–d. También incluye la validación del bloque T08-016.

## Resultado

- **Verificadas sin cambios en el compilador:** T08-016-03a, 03b, 03c, 03e y 04a–d.
- **T08-016-03d:** verificada en globales C. Queda pendiente la forma como campo de record (T08-016-03d1).

## Prueba

Nueva: `pruebas-tanda-08-arreglos-c/validar-arreglos.py`. Aprobada en O0–O3, con los rechazos en O0.

El mismo cuerpo se ejecuta en un control Clang, en el camino directo y en una plantilla precompilada consumida sin
encabezado (04d).

- **03a — arreglos de escalares.** Campos `int[4]` y `double[3]`, y globales `int[5]` y `float[2]`.
- **03b — matrices.** Campo `short[2][3]` y global `int[2][2]`.
- **03c — arreglos de records y enums.** Arreglos de struct, union y enum, en campos y en una global `Pt[2]`.
- **03d — arreglos de punteros calificados** (globales C).
  - Arreglo de punteros `const` (`int *const g_cps[2]`): escribir por el apuntado se admite; reasignar el elemento
    se rechaza («not assignable»).
  - Arreglo de punteros a `const` (`const int *g_pcs[2]`): reasignar el elemento se admite; escribir por el
    apuntado se rechaza.
  - Arreglo de punteros a `volatile` (`volatile int *g_pvs[2]`): el elemento conserva la calificación con `var`;
    quitarla se rechaza («would discard pointer pointee volatile qualification»).
- **03e — arreglos en records empaquetados y anidados.** `int vals[3]`, `In inner[2]` y `char bytes[3]` dentro de un
  struct `pack(1)`, y ese struct dentro de otro natural.
- **04a — layout.** Tamaños y offsets de cada arreglo, comparados con `sizeof`/`offsetof`/`_Alignof` de Clang.
  NativeSharp los mide con diferencias de direcciones; el paso de una fila de matriz y el de un elemento record
  coinciden.
- **04b — lecturas y escrituras cruzadas.** C llena y NativeSharp lee; NativeSharp escribe y C comprueba. Cubre
  campos, matrices, records, unions, enums y globales.
- **04c — dirección de elementos.**
  - Se admite la dirección de un elemento alineado (`&s.a[2]`) y la de un `char` empaquetado.
  - Se rechaza la de un `int` o un record desalineados del struct empaquetado: «operand of & must name addressable
    storage». El texto también menciona campos de bits, pero el rechazo es el del contrato vigente.

## Pendiente T08-016-03d1

Un arreglo de punteros calificados como campo de record no se importa:

- en el camino directo falta el miembro: «has no compatible member named 'cps'» (y `pcs`, `pvs`);
- en el precompilado se rompe la reconstrucción: «C record field cannot be reconstructed: …Arrs.cps».

Afecta a `int *const cps[2]`, `const int *pcs[2]` y `volatile int *pvs[2]`. Un arreglo de punteros sin calificar
(`int *ps[2]`) sí se importa y está cubierto por la prueba.

**Causa.** `projectCArrayField` rechaza los elementos puntero con apuntado `const` o `volatile`, o con el propio
elemento `const`. El campo importado tampoco tiene cómo llevar la calificación del apuntado a los elementos del
arreglo; las globales sí la llevan.

**Para cerrarlo hace falta:**

- proyectar esos arreglos en los campos;
- propagar `declaredPointerPointeeReadonly/Volatile` al indexar el campo, en el análisis de tipos y en el HIR;
- probar los tres caminos.

No se implementa en este bloque.

## Validación del bloque

- La prueba nueva.
- Las muestras previas T08-016-01 y 02: `global-static-matriz`, `bool-campos-c` y `flexible-c`, en la regresión de
  records ejecutada con el mismo compilador.

Ningún archivo del compilador cambió en este bloque.
