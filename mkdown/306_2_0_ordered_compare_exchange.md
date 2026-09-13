# 306 — Compare-exchange con órdenes de éxito y fallo / Ordered compare-exchange

## Resumen / Summary

Esta entrega añade órdenes de memoria independientes a `CompareExchange` para `AtomicInt32` y
`AtomicInt64`. La operación conserva el contrato existente: siempre devuelve el valor anterior,
tanto si el comparando coincide como si falla.

This delivery adds independent memory orders to `CompareExchange` for `AtomicInt32` and
`AtomicInt64`. The existing contract remains unchanged: the operation always returns the old
value whether the comparison succeeds or fails.

## Superficie pública / Public surface

```csharp
int AtomicInt32.CompareExchange(
    int value, int comparand, MemoryOrder successOrder, MemoryOrder failureOrder);
long AtomicInt64.CompareExchange(
    long value, long comparand, MemoryOrder successOrder, MemoryOrder failureOrder);
```

Las sobrecargas históricas de dos argumentos permanecen intactas y continúan siendo `seq_cst`.
The historical two-argument overloads remain intact and continue to be `seq_cst`.

## Reglas de ordenamiento / Ordering rules

- El orden de éxito admite Relaxed, Acquire, Release, AcquireRelease y SequentiallyConsistent.
- El orden de fallo admite sólo Relaxed, Acquire y SequentiallyConsistent.
- Fallo no admite Release ni AcquireRelease porque un intento fallido no realiza una escritura.
- El orden de fallo no puede ser más fuerte que el orden de éxito.
- Ambos órdenes deben resolverse como constantes `MemoryOrder` durante compilación.

La matriz aceptada es:

| Success | Failure permitido / Allowed failure |
| --- | --- |
| Relaxed | Relaxed |
| Acquire | Relaxed, Acquire |
| Release | Relaxed |
| AcquireRelease | Relaxed, Acquire |
| SequentiallyConsistent | Relaxed, Acquire, SequentiallyConsistent |

## Implementación / Implementation

### Frontend

`ProjectMethodBodyModel.cpp` reconoce exactamente la firma pública de cuatro argumentos. Pliega
ambos órdenes con el evaluador constante existente y diagnostica por separado:

- valores fuera del ABI público 0..4;
- órdenes de fallo Release/AcquireRelease;
- un orden de fallo más fuerte que éxito;
- selección de orden mediante una expresión de runtime.

El binding conserva `atomicMemoryOrder` para éxito y agrega `atomicFailureMemoryOrder` sólo para
esta operación. Ninguna otra operación atómica cambia su forma.

### HIR y MIR

`ProjectOopHirModel.cpp` consume los dos argumentos de orden y no los baja como valores de
runtime. El byte bajo de `integerValue` transporta éxito+1 y el byte siguiente fallo+1; cero
continúa identificando la sobrecarga histórica. Este empaquetado es metadata interna del pipeline
y no altera los valores públicos de `MemoryOrder`.

La conversión HIR a MIR ya copia `integerValue` de forma autoritativa, por lo que no fue necesario
agregar una segunda representación ni reinterpretar AST durante lowering.

### LLVM

`ProjectOopLlvmLowerer.cpp` decodifica ambos órdenes mediante los mapas explícitos existentes.
Emite `LLVMBuildAtomicCmpXchg` con `(comparand, value)` en el orden requerido por LLVM, alineación
natural de 4 u 8 bytes y extrae el elemento cero del resultado `{oldValue, success}`. Así conserva
el retorno NativeSharp del valor anterior sin ramas auxiliares.

## Archivos modificados / Modified files

- `lib/NativeSharp.Core.Threading/Atomics.ns`: sobrecargas públicas en 32 y 64 bits.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`: orden de fallo en el core binding.
- `src/Frontend/ProjectMethodBodyModel.cpp`: reconocimiento, plegado y matriz de validez.
- `src/Frontend/ProjectOopHirModel.cpp`: consumo estático y transporte compacto de ambos órdenes.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: emisión de `cmpxchg` ordenado y alineado.
- `tests/unit/Stage54_2AtomicsTests.cpp`: IR positivo y diagnósticos negativos.
- `examples/project_stage54_2/Main.ns`: éxito y fallo reales en ambos anchos.
- `docs/1.0.1/language/NativeSharp_Language_Specification_1_0_1.md`: NS-MEM-007.
- `docs/1.0.1/language/NativeSharp_1_0_1_conformance.map`: evidencia de conformidad.
- `docs/1.0.1/CURRENT_STATE.md`, `CHANGELOG.md` y `NATIVESHARP_2_0_ITEMS_FINAL.md`: estado.

## Evidencia enfocada / Focused evidence

```text
cmake --build A:\nsc-devel\build-tests --target nsc_stage54_2_atomics_tests -j 8
ctest --test-dir A:\nsc-devel\build-tests -R "^nsc.stage54-2.atomics$" --output-on-failure
1/1 passed
```

La prueba compila el fixture hasta LLVM, verifica combinaciones legales, exige diagnósticos para
cada clase ilegal y ejecuta el ejemplo empaquetado en O0/O2 según la cobertura histórica del test.
No se ejecutó baseline en esta entrega.

The test lowers the fixture through LLVM, checks legal combinations, requires diagnostics for each
illegal class, and runs the packaged example at O0/O2 under the test’s established coverage. No
baseline was run for this delivery.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Ordena compare-exchange por éxito y fallo / Order compare-exchange success and failure`

**Descripción / Description**

`Añade órdenes constantes e independientes a CompareExchange de 32/64 bits, valida la matriz legal y emite cmpxchg alineado conservando el valor anterior. / Add independent compile-time orders to 32/64-bit CompareExchange, validate the legal matrix, and emit aligned cmpxchg while preserving the old value.`
