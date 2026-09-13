# 271 — Seguridad, overflow y duración de VLAs / VLA safety, overflow, and lifetime

## Resumen / Summary

Las extensiones de los arreglos locales variables se validan antes de reservar stack: se rechazan
valores negativos, se comprueba cada producto antes de multiplicar y una declaración queda limitada
a 1 MiB. La procedencia del almacenamiento se conserva en aliases locales para impedir retornos,
campos y capturas que sobrevivan al frame.

Variable-length local-array extents are validated before reserving stack: negative values are
rejected, every product is checked before multiplication, and one declaration is capped at 1 MiB.
Storage provenance survives local aliases so returns, fields, and captures cannot outlive the frame.

## Archivos y funciones / Files and functions

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h` — `MethodLocalSymbol` conserva el tipo
  fuente de cada extensión y la raíz VLA de un alias de puntero.
- `src/Frontend/ProjectMethodBodyModel.cpp` — `analyzeVariable()`, `analyzeReturn()`,
  `analyzeAssignment()` y `collectLambdaCaptures()` forman y consumen la procedencia de frame.
- `src/Frontend/ProjectOopHirModel.cpp` — `lowerStatement()` mantiene las dimensiones separadas y
  con signo hasta que el backend pueda validarlas; el indexado convierte sólo el stride capturado.
- `src/LLVM/ProjectOopLlvmLowerer.cpp` — `StackAllocation` comprueba signo, producto y presupuesto
  antes del `alloca`, usando `AllocationSizeOverflow` como salida nativa uniforme.
- `tests/unit/VariableLengthArrayTests.cpp` — cubre tipos de extensión, retorno mediante alias,
  almacenamiento en campo, reasignación de la reserva y captura por lambda.
- `docs/1.0.1/` — actualiza especificación, libro, estado, changelog, mapas y roadmap.

## Implementación y decisiones / Implementation and decisions

Cada dimensión conserva su tipo integral fuente en el snapshot oculto. Esto permite comprobar
`signed >= 0` antes de convertir a `nuint`; convertir primero habría transformado un negativo en una
reserva positiva enorme. El nodo de reserva recibe cada factor por separado. LLVM calcula
`maximumElements / accumulatedProduct` y compara el siguiente factor antes de multiplicar, por lo
que el guard no depende de detectar un overflow después de que ya ocurrió. Si el producto es cero,
un divisor seguro mantiene definida la comprobación y el resultado final sigue siendo cero.

Each dimension keeps its source integral type in the hidden snapshot. This lets the backend check
`signed >= 0` before conversion to `nuint`; converting first would turn a negative into a huge
positive allocation. The allocation node receives every factor separately. LLVM compares the next
factor against `maximumElements / accumulatedProduct` before multiplying, so the guard never relies
on detecting overflow after it happened. A safe divisor keeps zero-sized arrays defined while the
final count remains zero.

El presupuesto de una sola declaración es 1 MiB, igual que el contrato previo de `stackalloc`.
Esta cota evita que una extensión válida aritméticamente destruya el frame. Las violaciones usan la
trampa existente `AllocationSizeOverflow`; no se añadió una excepción administrada ni una ruta de
heap implícita.

One declaration receives a 1 MiB budget, matching the existing `stackalloc` contract. This bound
prevents an arithmetically valid extent from destroying the frame. Violations use the existing
`AllocationSizeOverflow` trap; no managed exception or implicit heap fallback was introduced.

La procedencia se registra en el VLA y se propaga al copiarlo a otro puntero local. El compilador
rechaza retornos, escrituras no locales y capturas. Pasar el puntero a una llamada inmediata sigue
siendo válido porque es el uso prestado necesario para interoperabilidad nativa; el contrato del
callee no debe retenerlo.

Provenance is recorded on the VLA and propagated when copied to another local pointer. Returns,
non-local stores, and captures are rejected. Passing the pointer to an immediate call remains valid
because native interoperability needs that borrowed use; the callee contract must not retain it.

## Evidencia dirigida / Focused evidence

- La suite `nsc_variable_length_array_tests` pasa 19 comprobaciones.
- Un VLA bidimensional `2 x 3` compila en O0, ejecuta y retorna 42.
- Una dimensión `-1` termina con la razón `AllocationSizeOverflow`.
- Una reserva `1000000 x 3` de `int` termina con la misma razón antes del `alloca`.
- El IR contiene guards separados de signo y tamaño antes de los productos y una sola reserva.

No se ejecutó baseline para esta entrega. / No baseline was run for this delivery.

## Commit de referencia / Reference commit

**Título / Title**

`Endurece tamaño y duración de VLAs / Harden VLA sizing and lifetime`

**Descripción / Description**

`Valida dimensiones y productos antes de reservar stack, limita cada reserva al contrato de 1 MiB e impide retornos, campos y capturas que hagan escapar su almacenamiento. / Validates dimensions and products before stack allocation, caps each reservation to the 1 MiB contract, and prevents returns, fields, and captures from escaping its storage.`
