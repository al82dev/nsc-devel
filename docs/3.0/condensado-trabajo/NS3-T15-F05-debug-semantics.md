# NS3-T15-F05 — Depurabilidad semántica / Semantic debuggability

## Español

El compilador publica evidencia versionada y estable de nombres, tipos, disponibilidad, scopes, lifetimes, sequence points, inline sites y estado generado. La auditoría cubre locales, closures, variants, ownership, excepciones, coroutines y temporales. O0 exige fidelidad fuente y sequence points; O2 puede declarar un valor `OptimizedOut`, pero nunca inventarlo. El contrato describe la evidencia producida por NSC y no incorpora una interfaz de debugger.

Archivos modificados: `IntegrationQuality.h`, `IntegrationQuality.cpp` y `CMakeLists.txt`. Se creó `NativeSharp3T15DebugSemanticsTests.cpp` para verificar de forma puntual las invariantes O0/O2 y las ocho categorías observables.

## English

The compiler publishes versioned, stable evidence for names, types, availability, scopes, lifetimes, sequence points, inline sites, and generated state. The audit covers locals, closures, variants, ownership, exceptions, coroutines, and temporaries. O0 requires source fidelity and sequence points; O2 may report a value as `OptimizedOut`, but it must never invent one. This contract describes evidence produced by NSC and does not add a debugger interface.

Modified files: `IntegrationQuality.h`, `IntegrationQuality.cpp`, and `CMakeLists.txt`. `NativeSharp3T15DebugSemanticsTests.cpp` was added to verify O0/O2 invariants and all eight observable categories in isolation.
