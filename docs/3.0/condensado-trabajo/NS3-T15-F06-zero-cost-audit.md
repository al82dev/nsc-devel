# NS3-T15-F06 — Auditoría de costos ocultos / Hidden-cost audit

## Español

NSC clasifica allocation, copy, drop, dynamic dispatch, unwind edge y atomic fence por constructo fuente. Cada registro conserva evidencia MIR y assembly, modo de optimización y origen library-free. Un costo debe estar solicitado explícitamente por la semántica del constructo; cualquier excepción requiere una justificación no vacía y no puede confundirse con un costo explícito. La matriz exige evidencia con optimización desactivada y activada.

Archivos modificados: `IntegrationQuality.h`, `IntegrationQuality.cpp` y `CMakeLists.txt`. Se creó `NativeSharp3T15ZeroCostAuditTests.cpp` para validar puntualmente las seis clases de costo y los rechazos de evidencia incompleta.

## English

NSC classifies allocation, copy, drop, dynamic dispatch, unwind edge, and atomic fence costs by source construct. Every record retains MIR and assembly evidence, optimization mode, and a library-free origin. A cost must be explicitly requested by the construct semantics; any exception requires a nonempty rationale and cannot be confused with an explicit cost. The matrix requires evidence with optimization both disabled and enabled.

Modified files: `IntegrationQuality.h`, `IntegrationQuality.cpp`, and `CMakeLists.txt`. `NativeSharp3T15ZeroCostAuditTests.cpp` was added to validate all six cost classes and incomplete-evidence rejection in isolation.
