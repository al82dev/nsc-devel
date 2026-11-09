# T08-D23: arreglos de punteros calificados como campo de record C

Fecha: 2026-11-09. Decisión del usuario: T08-D23, resolver los arreglos de punteros calificados en campos. Casillas:
T08-016-03d y T08-016-03d1.

## Problema

Un arreglo de punteros calificados como campo de record C no se importaba. Afectaba a `int *const cps[2]`,
`const int *pcs[2]` y `volatile int *pvs[2]`.

- En directo faltaba el miembro: «has no compatible member named 'cps'».
- En el precompilado no se reconstruía el record: «C record field cannot be reconstructed».

Las globales C con esos tipos sí funcionaban (T08-016-03d).

## Causa

Había dos causas:

1. **Proyección.** `projectCArrayField` usa `cppAliasArrayType`, que rechaza un elemento puntero `const` o con
   dato apuntado `volatile`, y descarta además un dato apuntado `const`. La proyección del campo solo calculaba
   `pointeeReadonly`/`pointeeVolatile` para un campo puntero, no para un arreglo de punteros. Además, la regla de
   alineación efectiva rechazaba esos arreglos en un struct `pack(1)`.
2. **Modelo de miembros.** `setExperimentalCppInstanceFields` rechazaba las marcas del dato apuntado si la clave del
   tipo no terminaba en `*`. El arreglo `builtin:int*[2]` no termina en `*`.

El análisis de índices ya propagaba las marcas del dato apuntado a cada elemento (`qualifiedPointerElement`,
`volatilePointerElement`), igual que en las globales. No hizo falta cambiarlo.

## Cambio

**`CppSemanticRecord.cpp`.** Un campo C que es un arreglo (también una matriz) de punteros de un nivel se proyecta
cuando el elemento o el dato apuntado está calificado. El dato apuntado puede ser escalar, enum o record.

- El portador es el arreglo de punteros sin calificar (`builtin:int*[2]`).
- El `const` o `volatile` del dato apuntado pasa a `pointeeReadonly`/`pointeeVolatile` del campo.
- Un elemento `const` hace el campo de solo lectura.
- La regla de alineación efectiva admite esos arreglos en records empaquetados.

**`ProjectMemberModel.cpp`.** Las marcas del dato apuntado se admiten en un puntero o en un arreglo fijo de
punteros.

El precompilado reconstruye el record con la misma proyección. NSMeta no cambia.

## Prueba

Nueva: `pruebas-tanda-08-arreglos-c/validar-arreglos-punteros-campo.py`. Aprobada en O0–O3, con los rechazos en O0.

**Caminos:** control Clang, directo y plantilla precompilada consumida sin encabezado.

**Campos de `QP`:**

- `int *const cps[2]`;
- `const int *pcs[2]`, `volatile int *pvs[2]` y `const volatile int *pcvs[2]`;
- `const Pt *rps[2]`, `const Mode *eps[2]` y `const char *names[3]`;
- la matriz `const int *grid[2][2]`.

`PQ` es un struct `pack(1)` con `const int *pcs[2]` y `volatile int *pvs[1]`.

**Comprobaciones:**

- Los tamaños se comparan con Clang.
- C llena y NativeSharp lee; NativeSharp escribe y C comprueba. Así se cubren todos los offsets.
- Se escribe por un elemento puntero `const` y se reasignan los elementos que apuntan a `const`.
- `var` conserva `volatile`. En el IR, los stores por esos elementos son `store volatile`.

**Rechazos:**

- reasignar el elemento `const`;
- escribir por un dato `const`, también en un record, en un dato `const volatile` y en el struct empaquetado;
- quitar `const`: «would discard pointee constness»;
- quitar `volatile`: «would discard pointer pointee volatile qualification»;
- tomar la dirección de un elemento, igual que en las globales:
  - un puntero `const` sería `T* const*` (T08-D17);
  - un dato apuntado `const` o `volatile` da «cannot take the address of an array element with a … pointee».

`validar-arreglos.py` no cambia. Su comentario sobre la casilla pendiente T08-016-03d1 queda superado por esta
prueba.

## Validación

`A:\nsc-interop-build\claude\scripts\regresion-d23.cmd`, resultado 0:

- `arreglos-c`: `validar-arreglos-punteros-campo` y `validar-arreglos`;
- `punteros-c`: `validar-puntero-a-puntero-const` y `validar-punteros`;
- `layout-c`: `validar-layout` y `validar-alineacion-efectiva`;
- `callbacks-c\validar-callbacks`;
- `record-c`: `validar-miembros` y `validar.cmd`;
- `global-puntero-const\validar.cmd`.
