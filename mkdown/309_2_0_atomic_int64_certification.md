# 309 — Atomics nativos de 64 bits / Native 64-bit atomics

## Resumen / Summary

Esta entrega certifica que `NativeSharp.Core.Threading.AtomicInt64` ya cumple el contrato completo
del ítem 309. No introduce un segundo tipo ni duplica operaciones: formaliza la superficie existente,
prueba su comportamiento en todo el ancho de `long` y deja evidencia enfocada de su lowering nativo.

This delivery certifies that `NativeSharp.Core.Threading.AtomicInt64` already satisfies the complete
item 309 contract. It adds no duplicate type or operations: it formalizes the existing surface,
tests full-width `long` behavior, and records focused evidence for its native lowering.

## Superficie certificada / Certified surface

`AtomicInt64` conserva las operaciones históricas secuencialmente consistentes y las sobrecargas
ordenadas incorporadas en los ítems 304 a 306:

- `Load` y `Store`;
- `Increment`, `Decrement` y `Add`;
- `Exchange`;
- `CompareExchange` con orden único legacy o con órdenes separados de éxito y fallo;
- `Relaxed`, `Acquire`, `Release`, `AcquireRelease` y `SequentiallyConsistent` donde son legales.

Legacy operations remain sequentially consistent. Ordered overloads cover the five public memory
orders subject to the load, store, RMW, and compare-exchange legality rules already defined by
NS-MEM-005 through NS-MEM-007.

## Contratos de valor / Value contracts

Las operaciones utilizan los 64 bits completos de `long`. La prueba cruza el límite
`long.MaxValue -> long.MinValue`, utiliza operandos negativos y confirma que no existe truncamiento
a 32 bits. El overflow conserva la semántica definida de enteros NativeSharp.

`Increment`, `Decrement` y `Add` devuelven el valor nuevo. `Exchange` y `CompareExchange` devuelven
el valor anterior. Un compare-exchange fallido devuelve el valor observado y no modifica el
almacenamiento. Estos contratos no cambian con el orden de memoria elegido.

Operations use the complete 64-bit `long` representation. Increment, decrement, and add return the
new value; exchange and compare-exchange return the old value. A failed compare-exchange returns
the observed value and leaves storage unchanged. Memory order does not alter these value contracts.

## Diseño y decisión / Design and decision

La auditoría encontró que la implementación requerida ya existía desde Stage 54.2 y que los ítems
304 a 306 ya habían añadido las variantes ordenadas. Crear otra API habría dividido el contrato y
duplicado símbolos públicos sin aportar capacidad. Por ello este commit es una certificación:
documenta la garantía vigente y agrega una prueba integral específica del roadmap.

The audit found that the required implementation already existed since Stage 54.2, with ordered
forms added by items 304 through 306. Creating another API would split the contract and duplicate
public symbols without adding capability. This commit therefore certifies the existing design and
adds one comprehensive roadmap-specific test.

## Lowering y dependencias / Lowering and dependencies

En Windows x64, las cargas y stores ordenados se emiten como atomics LLVM `i64`; las operaciones
RMW usan `atomicrmw`; compare-exchange usa `cmpxchg`. Todas las instrucciones declaran alineación
natural de ocho bytes. Las operaciones legacy llegan a los hooks existentes del runtime, que
también se implementan con atomics LLVM nativos.

No se depende de exports `Interlocked*64` de Kernel32 ni de helpers externos `__atomic_*`. Esto
mantiene la implementación autocontenida dentro del compilador/runtime NativeSharp.

On Windows x64, ordered loads and stores lower to LLVM `i64` atomics, RMW operations use
`atomicrmw`, and compare-exchange uses `cmpxchg`, all naturally aligned to eight bytes. Legacy
runtime hooks also use native LLVM atomics. No Kernel32 `Interlocked*64` export or external
`__atomic_*` helper is required.

## Archivos creados / Created files

- `tests/unit/Item309AtomicInt64Tests.cpp`: prueba enfocada O0 de ancho, límites, órdenes, retornos,
  IR, enlace y ejecución.
- `mkdown/309_2_0_atomic_int64_certification.md`: registro bilingüe de la entrega.

## Archivos modificados / Modified files

- `CMakeLists.txt`: registra el ejecutable y la prueba `nsc.item309.atomic-int64`.
- `docs/1.0.1/language/NativeSharp_Language_Specification_1_0_1.md`: añade NS-MEM-010.
- `docs/1.0.1/compiler/NativeSharp_Compiler_Contract_1_0_1.md`: añade NSC-MEM-010.
- mapas de conformidad de lenguaje y compilador: enlazan el contrato con implementación y pruebas.
- `docs/1.0.1/CURRENT_STATE.md` y `docs/1.0.1/CHANGELOG.md`: publican el estado certificado.
- `docs/1.0.1/NATIVESHARP_2_0_ITEMS_FINAL.md`: marca el ítem 309 como completado.

No se modifican archivos de implementación del compilador o Core: la auditoría confirmó que los
caminos existentes ya satisfacen el requisito. El commit agrega la evidencia que faltaba.

No compiler or Core implementation file changes are needed: the audit confirmed that the existing
paths already satisfy the requirement. The commit adds the missing certification evidence.

## Evidencia enfocada / Focused evidence

```text
cmake --build A:\nsc-devel\build-tests --target nsc_item309_atomic_int64_tests -j 8
ctest --test-dir A:\nsc-devel\build-tests -R "^nsc.item309.atomic-int64$" --output-on-failure
1/1 passed
```

La prueba inspecciona IR O0 verifier-clean, ancho `i64`, `load atomic`, `store atomic`,
`atomicrmw`, `cmpxchg`, alineación 8 y ausencia de `__atomic_*`. El ejecutable comprueba límites,
negativos, overflow, CAS exitoso/fallido, formas legacy y los cinco órdenes públicos. No se ejecutó
baseline; corresponde únicamente al cierre de la tanda 5.

The focused test inspects verifier-clean O0 IR for i64 width, every atomic operation family,
eight-byte alignment, and absence of external helpers. Runtime coverage includes boundaries,
negative values, wraparound, successful/failed CAS, legacy forms, and all five public orders. No
baseline was run; the tanda 5 baseline remains deferred until the batch closes.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Certifica atomics nativos de 64 bits / Certify native 64-bit atomics`

**Descripción / Description**

`Formaliza AtomicInt64 sin duplicar su API y certifica ancho i64, alineación 8, overflow, retornos, órdenes y ausencia de helpers externos mediante una prueba O0 ejecutable. / Formalize AtomicInt64 without duplicating its API and certify i64 width, eight-byte alignment, overflow, return values, ordering, and absence of external helpers through an executable O0 test.`
