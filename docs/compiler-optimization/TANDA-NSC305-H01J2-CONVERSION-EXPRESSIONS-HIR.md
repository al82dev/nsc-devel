# NSC 3.0.5 — H01J2: conversion expressions HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Extraer `TypeTestExpression`, `SafeCastExpression` y `CastExpression` a `HirConversion.cpp`. |
| Naturaleza | Extracción de un bloque completo del dispatcher mediante una frontera privada. |
| Símbolo | `lowerConversionExpression`. |
| Protegido | Runtime-type binding, target ordinal/interface, tipo resultante, checked cast y conversiones de usuario. |
| Fuera de alcance | Cambios semánticos, nuevos casts, iteratividad H02 y optimización MIR/LLVM. |
| Terminación | Tres casos delegados, auditoría H01A–J, `nsc` construido y focos HIR 34/34 y 12/12. |

## Resultado

- `HirExpressionMachine.cpp` conserva los tres labels del dispatcher y delega a una sola frontera.
- `lowerConversionExpression` preserva las rutas de type-test, safe cast, checked cast, conversión
  de usuario mediante `lowerBoundCall` y conversión ordinaria.
- La ruta de conversión de usuario reenvía los mismos `expression`, `memberOrdinal`,
  `addressContext`, `typeBinding`, binding y argumento que la lambda previa.
- No se cambió sintaxis, diagnóstico público, API, ABI, formato, metadata ni contrato observable.

## Auditoría de cierre H01

- H01A–J tienen archivo físico, símbolo o frontera delegada correspondiente.
- Los dispatchers de statements conservan `suspend`/`yield` delegados a H01H y los exits delegados
  a H01I; no reapareció el hueco detectado durante H01H.
- Los dispatchers de expressions conservan delegaciones únicas para calls, construcción,
  aggregates/layout, destrucción, conversiones y await.
- `lowerDefaultArgument` permanece deliberadamente en H01C porque implementa parámetros
  opcionales, no clasificación general de conversiones.

## Evidencia focal

- `git diff --check`: limpio.
- `cmake --build A:/nsc-devel/build-test-infrastructure --target nsc_hir_tests nsc_namespace_function_hir_tests`: exit `0`.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato de runtime estático de `nsc.exe`: aprobado.

H01J y H01 quedan cerrados. Esta tanda no afirma validación integral ni ejecuta baseline, deploy o
batería completa.
