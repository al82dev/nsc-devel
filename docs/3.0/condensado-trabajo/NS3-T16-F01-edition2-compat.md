# NS3-T16-F01 — Modo de compatibilidad 2.x / 2.x compatibility mode

## Español

El frontend 3.0 acepta unidades de edición 2 únicamente con semántica 2.x explícita. Un módulo semántico no puede mezclar ediciones silenciosamente, aunque módulos de distintas ediciones sí pueden importarse mediante identidades canónicas. Metadata 2.x válida se acepta directamente cuando target y fingerprint ABI coinciden; un fingerprint distinto exige adapter explícito y una estructura corrupta se rechaza. Cambiar fuente o edición invalida el módulo y sus consumidores incrementales.

Archivos modificados: `CompatibilitySemantics.h`, `CompatibilitySemantics.cpp` y `CMakeLists.txt`. Se creó `NativeSharp3T16Edition2CompatTests.cpp` para aislar corpus mixto, metadata antigua, versiones inválidas e invalidación incremental.

## English

The 3.0 frontend accepts edition 2 units only with explicit 2.x semantics. A semantic module cannot silently mix editions, although modules from different editions may import each other through canonical identities. Valid 2.x metadata loads directly when target and ABI fingerprint match; a different fingerprint requires an explicit adapter, and corrupt structure is rejected. Changing source or edition invalidates the module and its incremental consumers.

Modified files: `CompatibilitySemantics.h`, `CompatibilitySemantics.cpp`, and `CMakeLists.txt`. `NativeSharp3T16Edition2CompatTests.cpp` was added to isolate mixed corpora, old metadata, invalid versions, and incremental invalidation.
