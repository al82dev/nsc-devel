# 402 — Ledger de breaking changes / Breaking-change ledger

## Español

NativeSharp 2.0 ya tiene un inventario canónico y verificable de incompatibilidades respecto de
1.0.1. `NativeSharp2BreakingChanges.ledger` usa el formato `NSBREAKING 1` y registra, para cada
cambio, el contrato 1.x afectado, consumidor, alternativas compatibles descartadas, semántica
nueva, impacto, diagnóstico y migración, versión mínima y pruebas de compatibilidad.

La única ruptura certificada es la separación del runtime Win32: una biblioteca estática ya no
duplica el runtime dentro de su producto y un host C o C++ debe enlazar explícitamente
`NativeSharp.Runtime.Win32.lib`. El ledger se genera con el compilador, se instala junto a los
contratos del toolchain y forma parte de la verificación de la instalación.

## English

NativeSharp 2.0 now has a canonical, verifiable inventory of incompatibilities relative to 1.0.1.
`NativeSharp2BreakingChanges.ledger` uses the `NSBREAKING 1` format and records, for every change,
the affected 1.x contract, consumer, rejected compatible alternatives, new semantics, impact,
diagnostic and migration, minimum version, and compatibility tests.

The only certified break is the independent Win32 runtime: a static library no longer duplicates
the runtime in its product, and a C or C++ host must explicitly link
`NativeSharp.Runtime.Win32.lib`. The compiler generates the ledger, the package installs it beside
the toolchain contracts, and installation verification validates it.
