# T08-015-11: condicional entre miembros flexibles C

Fecha: 2026-11-09. Pendiente hallado al validar T08-D13.

## Problema

`var p = c != 0 ? a->items : b->items;`, con `volatile int items[]`, se rechazaba con «operand of * must be a
non-void pointer». C lo acepta: cada rama decae a `volatile int *`. El condicional conservaba el tipo arreglo
`int[*]` y no marcaba el resultado como miembro flexible, así que `var` no podía inferir un puntero.

## Cambio

**Análisis del condicional** (`OperatorAndConditionalExpressions.cpp`).

- Una rama que es un miembro flexible C o un `extern T a[]` que puede decaer pasa a `T*` en dos casos:
  - la otra rama es un arreglo del mismo tipo;
  - la otra rama es un `T*`.
- Si la rama es `volatile`, inicia el contrato de alias C de T08-D13, igual que en un inicializador. El
  condicional une los dos contratos.
- Los demás pares siguen la regla anterior; por ejemplo, dos arreglos de distinto elemento se siguen rechazando.

**HIR** (`HirExpressionMachine.cpp`, `convertConditionalBranch`). Aplica a la rama la conversión
`CFlexibleArrayToPointer`, que toma la dirección del campo o de la global, como ya hacía con el inicializador de un
`var`.

## Prueba

Casos nuevos en `volatile-alias-c/validar-alias-modo.py`:

| Caso | Cuerpo |
|---|---|
| `Kflexcond` | `var p = c != 0 ? a->items : b->items; *p = 8; return p[1];` |
| `Kflexmix` | `var pb = b->items; var p = c != 0 ? a->items : pb; …` |
| `Kflexglobal` | `var p = c != 0 ? g_flex : a->items; …` |

**IR.** Se compara con Clang por función en Default, Microsoft e ISO: en O1 en todos los caminos (directo,
precompilado sin encabezado, referencias mixtas y grafo) y en O0–O3 en el directo. En Microsoft el acceso es
acquire/release y en ISO es `volatile` simple.

**Ejecución.** En O0–O3 el resultado coincide con el control Clang (234).

## Validación

`claude\scripts\regresion-d13.cmd`. El resultado es el mismo que en T08-D13: todo correcto salvo el diagnóstico
histórico `diagnosticar-var-volatile.cmd`, que termina con 1 por diseño.
