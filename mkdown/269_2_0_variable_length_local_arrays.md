# 269 — Arreglos locales de longitud variable / Variable-length local arrays

## Resumen / Summary

NativeSharp admite `T[length] local;` cuando `length` se evalúa en ejecución. La declaración crea
almacenamiento contiguo dentro del frame actual y permite indexarlo directamente, sin crear un
array de heap, sin cabecera y sin `delete`.

NativeSharp accepts `T[length] local;` when `length` is evaluated at runtime. The declaration creates
contiguous storage in the current frame and exposes direct indexing without creating a heap array,
an array header, or a matching `delete`.

## Archivos y funciones / Files and functions

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h` — `MethodLocalSymbol` conserva la marca
  VLA, el tipo de elemento y la expresión de extensión.
- `src/Frontend/ProjectMethodBodyModel.cpp` — `analyzeVariable()` distingue una extensión local de
  ejecución de un fixed array constante, valida tipo de contador, elemento y posición sintáctica.
- `src/Frontend/ProjectOopHirModel.cpp` — `lowerStatement()` sintetiza la operación
  `StackAllocation` en el punto exacto de la declaración.
- `src/Frontend/ProjectOopMirModel.cpp` y `src/LLVM/ProjectOopLlvmLowerer.cpp` — se reutiliza sin
  duplicación la ruta existente que obtiene tamaño/alineación canónicos y emite `alloca`.
- `tests/unit/VariableLengthArrayTests.cpp` — prueba declaración dinámica, almacenamiento fijo no
  regresado, indexado raw y rechazos de campo y extensión no integral.
- `CMakeLists.txt`, `docs/1.0.1/` — registran la evidencia y el contrato público actual.

## Implementación y decisiones / Implementation and decisions

No se añadió una segunda primitiva de backend. El símbolo local conserva que la fuente declaró un
VLA, pero usa una representación interna de puntero al elemento; HIR crea como inicializador la
misma `StackAllocation` usada por `stackalloc T[n]`. MIR obtiene `sizeof(T)` y `alignof(T)` desde
`ProjectObjectLayoutModel`, y LLVM reserva bytes alineados en la ubicación léxica de la declaración.

No second backend primitive was added. The local symbol preserves that source declared a VLA while
using an internal element-pointer representation; HIR supplies the same `StackAllocation` used by
`stackalloc T[n]`. MIR obtains `sizeof(T)` and `alignof(T)` from `ProjectObjectLayoutModel`, and LLVM
allocates aligned bytes at the declaration's lexical position.

Esta forma se limita deliberadamente a locales y a una dimensión de ejecución en este commit. Los
campos necesitan layout estable, y parámetros/retornos necesitan ABI estable. Las dimensiones
múltiples y el endurecimiento completo de overflow/lifetime pertenecen a los items 270 y 271 para
que cada decisión tenga evidencia y commit independiente.

This form is deliberately limited to locals and one runtime dimension in this commit. Fields require
stable layout, and parameters/returns require stable ABI. Multiple dimensions and complete
overflow/lifetime hardening belong to items 270 and 271 so each decision has independent evidence
and history.

## Evidencia dirigida / Focused evidence

- `int[length] values` llega a MIR con una sola `StackAllocation`.
- Lecturas y escrituras producen `RawElementAddress` con el stride de `int`.
- `int[4]` conserva su identidad fixed-array y no se reclasifica.
- Una extensión `double` y un campo con extensión de ejecución se rechazan.
- La prueba enfocada completó 9 comprobaciones.

No se ejecutó baseline para esta entrega. / No baseline was run for this delivery.

## Commit de referencia / Reference commit

**Título / Title**

`Añade arreglos locales de longitud variable / Add variable-length local arrays`

**Descripción / Description**

`Permite T[length] en variables locales, reutiliza la reserva stackalloc y el indexado raw, y mantiene las extensiones de ejecución fuera de campos y firmas. / Enables T[length] for local variables, reuses stackalloc storage and raw indexing, and keeps runtime extents out of fields and signatures.`
