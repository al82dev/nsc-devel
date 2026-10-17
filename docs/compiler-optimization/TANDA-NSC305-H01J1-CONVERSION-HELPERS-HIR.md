# NSC 3.0.5 — H01J1: conversion and borrow helpers HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Extraer los helpers compartidos de conversión y préstamos a `HirConversion.cpp`. |
| Naturaleza | Movimiento mecánico de cuatro definiciones privadas completas. |
| Símbolos | `wrapImplicitConversion`, `wrapCallArgumentBorrow`, `wrapFreshOwnedTemporaryBorrow` y `classifyImplicit`. |
| Protegido | Conversiones implícitas y de usuario, vistas, temporales owned, bindings de borrow y clasificación semántica. |
| Fuera de alcance | Casos de expresión `is`/`as`/cast, argumentos opcionales e iteratividad H02. |
| Terminación | Definiciones únicas, `nsc` construido y focos HIR 34/34 y 12/12. |

## Resultado

- Se movieron sin reescritura 134 líneas de `HirBuildCoordinator.cpp`.
- `HirConversion.cpp` contiene 144 líneas, incluidos sus includes y namespace explícitos.
- `lowerDefaultArgument` permanece en `HirFunctionLowerer.cpp`: pertenece al contrato de
  parámetros opcionales cerrado en H01C y no es necesario moverlo para separar los wrappers.
- No se cambió sintaxis, diagnóstico público, API, ABI, formato, metadata ni contrato observable.

## Evidencia focal

- `git diff --check`: limpio.
- `cmake --build A:/nsc-devel/build-test-infrastructure --target nsc_hir_tests nsc_namespace_function_hir_tests`: exit `0`.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato de runtime estático de `nsc.exe`: aprobado.

H01J permanece abierto para extraer los casos de expresión `TypeTestExpression`,
`SafeCastExpression` y `CastExpression`. Esta tanda no afirma validación integral ni ejecuta
baseline, deploy o batería completa.
