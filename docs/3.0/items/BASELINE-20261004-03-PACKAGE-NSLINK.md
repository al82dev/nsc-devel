# Baseline 2026-10-04: cierre de paquetes con NSLink

## Problema

La prueba de coherencia de paquetes forzaba `--linker lld-link` dentro de una compilación de
NativeSharp. Eso contradice la frontera establecida para 3.0: LLD enlaza el host C++ del compilador,
pero los artefactos `.ns` destinados al usuario se validan con NSLink.

## Cambio

El consumidor precompilado de `NativeSharp.Core.IO` solicita `--linker nslink`. Conserva las mismas
comprobaciones de restore, lock, procedencia y ejecución.

## Archivo

- `tests/unit/NativeSharp3T33PackageCoherenceTests.cpp`

## Mejora

La prueba ya mide la ruta de distribución real y deja de certificar accidentalmente un linker que
no forma parte del flujo final del usuario.
