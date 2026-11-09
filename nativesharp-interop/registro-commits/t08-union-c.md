# T08-011: union C

Fecha: 2026-11-08. Casillas: T08-011-02a–d y T08-011-03a–c. También incluye la validación del bloque T08-011.

## Resultado

Verificadas sin cambios en el compilador. Queda la decisión pendiente T08-011-02c1.

## Prueba

Nueva: `pruebas-tanda-08-union-c/validar-uniones.py`. Aprobada en O0–O3, con el rechazo en O0.

El mismo cuerpo se ejecuta en un control Clang, en el camino directo con el encabezado y en una plantilla de un
productor precompilado consumida sin encabezado (03c).

- **02a — union natural.** `int`, `float`, `double`, `void *`, `long long` y `unsigned char[8]`. Se escribe por
  un miembro y se lee por otro, y los patrones se comparan con funciones C que hacen el mismo juego de tipos:
  `float_bits` y `double_bits`.
- **02b — union empaquetada.** Una union `pack(1)` dentro de un struct `pack(1)`: la union queda en el offset 1
  y el campo siguiente en el 9.
  - C escribe y NativeSharp lee; NativeSharp escribe y C comprueba.
  - NativeSharp rechaza la dirección de un miembro desalineado (contrato vigente, igual que en
    `flexible-c/RestrictedPackedAddress`). Por eso los offsets se comprueban leyendo los bytes del struct.
- **02c — union dentro de struct y struct dentro de union.** El struct dentro de la union tiene etiqueta.
- **02d — union anónima.** Tiene miembros promovidos (`all`, `real`) y campos de bits superpuestos (`lo`, `mid`,
  `top`). Se escribe por los campos de bits y se lee el entero; después se escribe el `float` y se leen los
  campos.
- **03a — layout.** `sizeof`, `_Alignof` y `offsetof` compilados por Clang. NativeSharp mide los tamaños con
  `sizeof`, y los offsets y la alineación con diferencias de direcciones.
- **03b — bytes.** Se comprueban los bytes concretos tras cada escritura cruzada.

**Rechazo:** tomar la dirección de un campo de bits promovido desde la union anónima da «bit fields have no
address».

## Decisión pendiente T08-011-02c1

**Situación.** Un miembro con nombre cuyo tipo es un struct o union sin etiqueta no se importa. Por ejemplo,
`union U3 { struct { short lo; short hi; } pair; int whole; }` o el mismo patrón dentro de un struct.

- En el camino directo, el miembro falta: «has no compatible member named 'pair'».
- En el camino precompilado, si una plantilla publicada usa el record, falla la generación de la biblioteca:
  «metadata producer read-back verification failed: C record field dependency is unavailable or
  incompatible».

**Causa.** La sesión de Clang no inventaría records sin nombre que no tengan un typedef. Los únicos que se
manejan son los portadores anónimos, cuyos miembros se promueven.

**Opciones.** Los efectos para el usuario son distintos en cada una:

1. Dar a esos tipos una identidad sintetizada (por ejemplo, propietario más miembro) e importarlos. La metadata
   tendría que llevar esa identidad.
2. Omitir el miembro de forma coherente en los dos caminos. La biblioteca se genera y el resto de los miembros
   queda disponible.
3. Rechazarlo con un diagnóstico explícito.

La prueba usa por ahora un struct con etiqueta dentro de la union.

## Validación del bloque

- La prueba nueva.
- La muestra previa T08-011-01: `pruebas-tanda-08-union/validar.cmd`, ejecutada en la validación del bloque
  T08-010 con el mismo compilador.
- `bitfields-c/validar-union.cmd`.

Ningún archivo del compilador cambió en este bloque.
