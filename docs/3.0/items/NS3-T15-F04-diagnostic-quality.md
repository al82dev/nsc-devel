# NS3-T15-F04 — Calidad de diagnósticos / Diagnostic quality

## Español

El gate de calidad exige códigos estables y únicos, mensajes en inglés y español, spans físicos precisos, cascadas de máximo dos diagnósticos y cobertura explícita de errores de borrow, traits, match, async y módulos. Un fix-it sólo puede publicarse si es compilable y preserva semántica. Los backtraces de macros se limitan a cuatro marcos y el renderizador acota cada línea al ancho de terminal indicado.

Archivos modificados: `IntegrationQuality.h`, `IntegrationQuality.cpp` y `CMakeLists.txt`. Se creó `NativeSharp3T15DiagnosticQualityTests.cpp` porque el contrato necesita una prueba puntual independiente y auditable.

## English

The quality gate requires stable unique codes, English and Spanish messages, precise physical spans, at most two cascaded diagnostics, and explicit coverage of borrow, trait, match, async, and module errors. A fix-it may only be published when it compiles and preserves semantics. Macro backtraces are limited to four frames, and the renderer bounds every line to the requested terminal width.

Modified files: `IntegrationQuality.h`, `IntegrationQuality.cpp`, and `CMakeLists.txt`. `NativeSharp3T15DiagnosticQualityTests.cpp` was added because this contract needs an independent, auditable targeted test.
