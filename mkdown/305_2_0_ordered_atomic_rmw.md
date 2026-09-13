# Entrega 305 — Operaciones RMW ordenadas
# Delivery 305 — Ordered read-modify-write operations

## Resultado / Outcome

`AtomicInt32` y `AtomicInt64` incorporan orden explícito en `Increment`, `Decrement`, `Add` y
`Exchange`. Las sobrecargas anteriores siguen siendo secuencialmente consistentes.

`AtomicInt32` and `AtomicInt64` now provide explicitly ordered `Increment`, `Decrement`,
`Add`, and `Exchange`. Existing overloads remain sequentially consistent.

## API pública / Public API

Cada ancho publica:

- `Increment(MemoryOrder)`
- `Decrement(MemoryOrder)`
- `Add(value, MemoryOrder)`
- `Exchange(value, MemoryOrder)`

Las cuatro operaciones aceptan `Relaxed`, `Acquire`, `Release`, `AcquireRelease` y
`SequentiallyConsistent`. El argumento debe ser constante de compilación.

All four operations accept the five public orders. The argument must be a compile-time constant.

## Semántica / Semantics

- `Increment`, `Decrement` y `Add` devuelven el valor nuevo.
- `Exchange` devuelve el valor anterior.
- Los enteros usan wrap nativo; decrement se expresa como suma atómica de menos uno.
- Las instrucciones llevan alineación 4 para `int` y 8 para `long`.

- Increment, Decrement, and Add return the new value.
- Exchange returns the old value.
- Integer arithmetic keeps native wrapping semantics.
- Atomic instructions carry 4-byte or 8-byte alignment.

## Implementación / Implementation

- `ProjectMethodBodyModel` amplía el reconocimiento exacto de firmas a las ocho sobrecargas.
- La validación compartida admite todo el rango ABI 0..4 para RMW y rechaza valores externos.
- `ProjectOopHirModel` reutiliza `atomicMemoryOrder`: el argumento se pliega y no se evalúa en
  runtime.
- `orderedAtomicRmwOrdering` traduce explícitamente los cinco valores a LLVM.
- `buildOrderedAtomicRmw` centraliza `atomicrmw add/xchg`, alineación y reconstrucción del valor
  nuevo cuando el IR devuelve el anterior.
- Las rutas legacy hacia los hooks `seq_cst` permanecen intactas.

The implementation reuses the compile-time metadata channel introduced by item 304. A shared LLVM
helper emits aligned `atomicrmw` operations and reconstructs the public return contract without
introducing runtime order switches.

## Archivos y funciones / Files and functions

- `lib/NativeSharp.Core.Threading/Atomics.ns`
  - ocho sobrecargas públicas nuevas.
- `include/NativeSharp/Frontend/ProjectMethodBodyModel.h`
  - generaliza la documentación de `atomicMemoryOrder`.
- `src/Frontend/ProjectMethodBodyModel.cpp`
  - `orderedUnaryRmw`, `orderedValueRmw` y validación ABI.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
  - `orderedAtomicRmwOrdering`, `buildOrderedAtomicRmw` y ocho rutas ordenadas.
- `tests/unit/Stage54_2AtomicsTests.cpp`
  - cubre firmas, órdenes, IR `acq_rel`, valor inválido y ejecución.
- `examples/project_stage54_2/Main.ns`
  - añade diez comprobaciones; el total pasa de 38 a 48.
- Documentos 1.0.1
  - agregan `NS-MEM-006`, conformidad, estado, changelog y roadmap.

## Evidencia enfocada / Focused evidence

```text
cmake --build A:\nsc-devel\build-tests --target nsc_stage54_2_atomics_tests -j 8
ctest --test-dir A:\nsc-devel\build-tests -R "^nsc.stage54-2.atomics$" --output-on-failure
1/1 passed
```

El ejemplo se ejecuta en O0 y O2 dentro de esta prueba enfocada. No se ejecutó baseline.

The focused test executes the example at O0 and O2. No baseline was run.

## Decisiones / Decisions

- Se reutiliza el mismo enum estable; no se crean enums por operación.
- El orden sigue siendo estático; no hay switch en runtime.
- `AcquireRelease` es legal para RMW.
- CompareExchange queda deliberadamente para el item 306 porque necesita órdenes separadas de éxito
  y fallo.

## Mensaje de commit de referencia / Reference commit message

Título / Title:

`Ordena las operaciones atómicas RMW / Order atomic RMW operations`

Descripción / Description:

`Añade MemoryOrder a Increment, Decrement, Add y Exchange en 32/64 bits, conserva sus contratos de retorno y emite atomicrmw alineado para los cinco órdenes. / Add MemoryOrder to 32/64-bit Increment, Decrement, Add, and Exchange, preserve their return contracts, and emit aligned atomicrmw for all five orders.`
