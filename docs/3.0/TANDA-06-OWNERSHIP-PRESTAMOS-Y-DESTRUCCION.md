# Tanda 06 — Ownership, préstamos y destrucción / Ownership, borrows, and destruction

## Resultado / Outcome

ES: Se formalizó copy/move, moves parciales, drop flags, préstamos divididos, lifetimes, cleanup,
pinning y summaries de escape versionados. EN: Copy/move, partial moves, drop flags, split borrows,
lifetimes, cleanup, pinning, and versioned escape summaries now have deterministic contracts.

## Archivos / Files

- Nuevos `OwnershipSemantics.h/.cpp`: máquina de estados y contratos interprocedurales T06.
- Nueva `NativeSharp3T06OwnershipTests.cpp`: once comprobaciones específicas.
- `CMakeLists.txt`: incorpora implementación y target aislado.

## Mejora / Improvement

ES: Un valor movido ya no es observable; los drops parciales conservan exactamente los campos
vivos; todas las salidas comparten orden LIFO; pinning impide relocalización; y los límites DLL
pueden transportar un summary de escape que falla cerrado si está corrupto.

EN: Moved values are unobservable, partial drops retain exactly live fields, every exit shares LIFO
cleanup, pinning prevents relocation, and DLL boundaries can carry a fail-closed escape summary.

## Límite / Boundary

T06 congela el modelo y formato `escape-v1`. La inserción completa de drop flags y cleanups en MIR,
además de su publicación NSMETA, se conecta en T15. T15 owns full MIR/NSMETA integration.

## Validación / Validation

`nsc` O0 y `nsc_ns3_t06_ownership_tests` directo; nunca se ejecutó baseline o CTest general.
