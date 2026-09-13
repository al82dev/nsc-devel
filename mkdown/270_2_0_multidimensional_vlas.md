# 270 — VLAs multidimensionales / Multidimensional VLAs

## Resumen / Summary

Los arreglos locales de longitud variable admiten `T[rows, columns, ...]`. Todas las dimensiones
forman una sola reserva contigua row-major; el indexado por rank se linealiza con los strides
capturados al ejecutar la declaración.

Variable-length local arrays accept `T[rows, columns, ...]`. Every dimension forms one contiguous
row-major reservation; ranked indexing is linearized with strides captured when the declaration
executes.

## Archivos y funciones / Files and functions

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h` — `MethodLocalSymbol` conserva todas las
  extensiones y sus locales ocultos; `MethodVariableArrayIndexBinding` conecta un acceso con ellas.
- `src/Frontend/ProjectMethodBodyModel.cpp` — `analyzeVariable()` resuelve las dimensiones en orden
  fuente, crea snapshots `nuint` y valida el rank en cada `IndexExpression`.
- `src/Frontend/ProjectOopHirModel.cpp` — `lowerStatement()` evalúa cada dimensión una sola vez,
  compone el total de elementos y linealiza `array[i0, i1, ...]` antes del acceso raw.
- `tests/unit/VariableLengthArrayTests.cpp` — amplía la evidencia a dos dimensiones, snapshots,
  una sola reserva, acceso aplanado y rank incorrecto.
- `docs/1.0.1/` — especificación, libro, estado, changelog, mapas y roadmap.

## Implementación y decisiones / Implementation and decisions

El parser ya normaliza `T[d0, d1]` como modificadores anidados en orden interno. El modelo de método
los devuelve a orden fuente y crea un local oculto `nuint` por dimensión. HIR declara esos snapshots
antes del buffer y calcula el total con su producto. Esto garantiza evaluación única y evita que
`columns = otra;` cambie después el stride de una matriz ya reservada.

The parser already normalizes `T[d0, d1]` into nested modifiers in internal order. The method model
restores source order and creates one hidden `nuint` local per dimension. HIR declares those
snapshots before the buffer and computes the total through their product. This guarantees single
evaluation and prevents a later `columns = other;` from changing an existing matrix's stride.

El acceso usa la fórmula row-major incremental
`linear = linear * capturedExtent[dimension] + index`. Después emite un único
`RawElementAddress` del elemento base, por lo que MIR/LLVM conservan la misma autoridad de tamaño y
alineación que el VLA unidimensional y `stackalloc`.

Access uses the incremental row-major formula
`linear = linear * capturedExtent[dimension] + index`. It then emits one base-element
`RawElementAddress`, keeping MIR/LLVM on the same size and alignment authority used by one-dimensional
VLAs and `stackalloc`.

## Evidencia dirigida / Focused evidence

- Dos dimensiones producen dos snapshots `nuint` y una sola `StackAllocation`.
- Cuatro accesos multidimensionales producen cuatro direcciones raw aplanadas.
- Modificar la variable `columns` después de declarar no sustituye el snapshot del stride.
- Un acceso con menos índices que dimensiones se rechaza.
- La suite enfocada amplió su resultado de 9 a 15 comprobaciones.

No se ejecutó baseline para esta entrega. / No baseline was run for this delivery.

## Commit de referencia / Reference commit

**Título / Title**

`Añade VLAs multidimensionales row-major / Add row-major multidimensional VLAs`

**Descripción / Description**

`Captura cada dimensión una vez, compone una sola reserva contigua y linealiza array[i0, i1, ...] con strides dinámicos estables. / Captures every dimension once, composes one contiguous reservation, and linearizes array[i0, i1, ...] with stable dynamic strides.`
