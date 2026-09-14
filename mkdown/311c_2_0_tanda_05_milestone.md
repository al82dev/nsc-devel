# Milestone — NativeSharp 2.0 tanda 5 / Batch 5 milestone

## Alcance / Scope

La tanda 5 completa los ítems 303–311 del roadmap y entrega el modelo de memoria concurrente
nativo: órdenes de memoria públicas, load/store y operaciones RMW ordenadas, compare-exchange con
órdenes independientes de éxito y fallo, fences de compilador y CPU, memoria volatile para MMIO,
atomics de 64 bits y punteros, y la API pública Interlocked. / Batch 5 completes roadmap items
303–311 and delivers the native concurrent memory model: public memory orders, ordered load/store
and RMW operations, compare-exchange with independent success and failure orders, compiler and CPU
fences, volatile MMIO memory, 64-bit and pointer atomics, and the public Interlocked API.

El cierre incluye NSMETA 1.14 para conservar volatile sin confundirlo con atomicidad, además del
golden histórico NSMETA 1.13 y la alineación de las pruebas que consumen el formato vigente. / The
closure includes NSMETA 1.14 to preserve volatile without confusing it with atomicity, plus the
historical NSMETA 1.13 golden and aligned tests that consume the current format.

## Entregas / Deliveries

- 303: órdenes Relaxed, Acquire, Release, AcquireRelease y SequentiallyConsistent estables. /
  Stable Relaxed, Acquire, Release, AcquireRelease, and SequentiallyConsistent orders.
- 304–306: load/store, RMW y compare-exchange ordenados con validación semántica. / Ordered
  load/store, RMW, and compare-exchange with semantic validation.
- 307: fences explícitos de compilador y CPU. / Explicit compiler and CPU fences.
- 308: volatile de campos escalares, enum y puntero para MMIO, preservado hasta LLVM y NSMETA. /
  Field-only scalar, enum, and pointer volatile for MMIO, preserved through LLVM and NSMETA.
- 309–310: atomics nativos de 64 bits y punteros. / Native 64-bit and pointer atomics.
- 311: superficie pública Interlocked sobre las mismas operaciones certificadas. / Public
  Interlocked surface over the same certified operations.

## Baseline certificado / Certified baseline

```text
Run: 20260913-203621-c16bf2cf
Compiler build: 1.0.1-dev+build.20260914T023622Z.d211cbe09296a915
Compiler SHA-256: 82490bce96c845a6725198ebd5b86ae48b801b8d2df7bfb9132175ec0a636dd4
lld-link CTest: 344/344 passed
NSLink CTest: 344/344 passed
lld-link fixtures: 148/148 passed (O0/O2)
NSLink fixtures: 148/148 passed (O0/O2)
lld-link bug triage: 18/18 passed (O0/O2)
NSLink bug triage: 18/18 passed (O0/O2)
Gate 0: LANDED
Elapsed: 310.681 s
```

La evidencia canónica queda en `_abi/gates/G00/gate.json`; el historial detallado es regenerable y
no forma parte del snapshot. / Canonical evidence lives in `_abi/gates/G00/gate.json`; the detailed
run history is regenerable and is not part of the snapshot.

## Decisión de snapshot / Snapshot decision

El branch `nativesharp-2.0-tanda-05` apunta al commit que incorpora esta evidencia. El trabajo
permanece sobre `development`; el branch es un hito inmutable. / The
`nativesharp-2.0-tanda-05` branch points at the commit carrying this evidence. Work remains on
`development`; the branch is an immutable milestone.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Certifica el milestone de la tanda 5 / Certify the batch 5 milestone`

**Descripción / Description**

`Registra el Gate 0 verde para el modelo de memoria concurrente con 344 pruebas, 148 fixtures O0/O2 y 18 casos de triage por cada linker, y crea el snapshot inmutable de la tanda 5. / Record the green Gate 0 for the concurrent memory model with 344 tests, 148 O0/O2 fixtures, and 18 triage cases per linker, and create the immutable batch 5 snapshot.`
