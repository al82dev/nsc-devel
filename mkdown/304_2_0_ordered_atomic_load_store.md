# Entrega 304 — Load/store atómicos ordenados
# Delivery 304 — Ordered atomic loads and stores

## Resultado / Outcome

NativeSharp expone orden explícito para cargas y almacenes de `AtomicInt32` y `AtomicInt64`.
Las sobrecargas sin parámetro conservan `seq_cst`; la nueva superficie no cambia código existente.

NativeSharp now exposes explicit ordering for `AtomicInt32` and `AtomicInt64` loads and stores.
Parameterless overloads remain `seq_cst`, so existing source and ABI behavior are preserved.

## Superficie pública / Public surface

- `AtomicInt32.Load(MemoryOrder)`
- `AtomicInt32.Store(int, MemoryOrder)`
- `AtomicInt64.Load(MemoryOrder)`
- `AtomicInt64.Store(long, MemoryOrder)`

Las cargas aceptan `Relaxed`, `Acquire` y `SequentiallyConsistent`.
Los almacenes aceptan `Relaxed`, `Release` y `SequentiallyConsistent`.
El orden debe ser constante de compilación.

Loads accept `Relaxed`, `Acquire`, and `SequentiallyConsistent`.
Stores accept `Relaxed`, `Release`, and `SequentiallyConsistent`.
The order must be a compile-time constant.

## Implementación / Implementation

- `ProjectMethodBodyModel` reconoce las cuatro firmas por tipo, nombre y firma nominal exactos.
- `foldIntegerConstant` resuelve el enum y rechaza órdenes dinámicas o ilegales.
- `MethodCoreBinding::atomicMemoryOrder` conserva el valor ABI NativeSharp, nunca un ordinal LLVM.
- `ProjectOopHirModel` elimina el argumento de orden de los operandos ejecutables y lo transporta
  como metadata `integerValue + 1`; cero continúa identificando las sobrecargas legacy.
- `ProjectOopMirModel` conserva esa metadata mediante su copia normal de `integerValue`.
- `ProjectOopLlvmLowerer` traduce explícitamente:
  - `Relaxed` a LLVM `monotonic`.
  - `Acquire` a LLVM `acquire`.
  - `Release` a LLVM `release`.
  - `SequentiallyConsistent` a LLVM `seq_cst`.
- Las instrucciones declaran alineación 4 u 8. Sin ella, LLVM x64 introducía referencias externas
  a `__atomic_load` y `__atomic_store`; la alineación natural mantiene lowering nativo.

The frontend folds and validates the order once. HIR/MIR carry NativeSharp ABI metadata instead of
a runtime integer. LLVM lowering performs an explicit mapping and attaches natural alignment,
preventing accidental coupling to LLVM enum ordinals and unwanted `__atomic_*` library calls.

## Archivos / Files

- `include/NativeSharp/Frontend/ProjectMethodBodyModel.h`
  - agrega `MethodCoreBinding::atomicMemoryOrder`.
- `src/Frontend/ProjectMethodBodyModel.cpp`
  - reconoce firmas, pliega constantes y diagnostica órdenes inválidas.
- `src/Frontend/ProjectOopHirModel.cpp`
  - separa operandos ejecutables de metadata de orden.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
  - emite load/store atómicos con orden y alineación.
- `lib/NativeSharp.Core.Threading/Atomics.ns`
  - publica las cuatro sobrecargas.
- `examples/project_stage54_2/Main.ns`
  - añade seis comprobaciones ejecutables; el total pasa de 32 a 38.
- `tests/unit/Stage54_2AtomicsTests.cpp`
  - verifica IR, ejecución y diagnósticos negativos.
- `docs/1.0.1/language/NativeSharp_Language_Specification_1_0_1.md`
  - define `NS-MEM-005`.
- `docs/1.0.1/language/NativeSharp_1_0_1_conformance.map`
  - enlaza implementación y evidencia.
- `docs/1.0.1/CURRENT_STATE.md`, `CHANGELOG.md` y
  `NATIVESHARP_2_0_ITEMS_FINAL.md`
  - actualizan estado, cambios y roadmap.

## Evidencia enfocada / Focused evidence

```text
cmake --build A:\nsc-devel\build-tests --target nsc_stage54_2_atomics_tests -j 8
ctest --test-dir A:\nsc-devel\build-tests -R "^nsc.stage54-2.atomics$" --output-on-failure
1/1 passed
```

La prueba certifica `monotonic`, `acquire`, `release` y `seq_cst`, ambos anchos, ejecución O0/O2
del ejemplo y los tres rechazos: Release en load, Acquire en store y orden elegido en runtime.

## Decisiones / Decisions

- No se reutilizan ordinales LLVM como ABI público.
- No se debilitan las sobrecargas existentes.
- No se permite un switch de orden en runtime: el orden forma parte de la instrucción.
- `AcquireRelease` permanece en el vocabulario para RMW/CAS, pero no es válido en load/store.
- No se ejecutó baseline; corresponde únicamente al cierre de la tanda 5.

## Mensaje de commit de referencia / Reference commit message

Título / Title:

`Implementa load/store atómicos ordenados / Implement ordered atomic loads and stores`

Descripción / Description:

`Añade sobrecargas ordenadas para AtomicInt32/AtomicInt64, valida órdenes constantes legales y emite LLVM nativo alineado sin alterar las operaciones seq_cst existentes. / Add ordered AtomicInt32/AtomicInt64 overloads, validate legal constant orders, and emit aligned native LLVM without changing existing seq_cst operations.`
