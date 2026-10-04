# Tanda correctiva: identidad precompilada y resumen de escape

## Problema

`PrecompiledMemberIdentity` incorporó `parameterEscapeSummary`, pero su prueba unitaria conservó
seis inicializadores agregados con sólo los cinco campos anteriores. Con advertencias tratadas como
errores, Clang rechazaba la construcción completa del árbol CTest antes de ejecutar pruebas.

## Cambio

`PrecompiledMemberIdentityIndexTests.cpp` inicializa explícitamente el resumen de escape vacío en
las seis identidades. El vacío es deliberado: esta prueba valida indexación y ambigüedad de firmas,
no contratos interprocedurales de escape.

## Archivos

- Modificado: `tests/unit/PrecompiledMemberIdentityIndexTests.cpp`.
- Nuevo: este documento de trazabilidad.

## Mejora

El fixture vuelve a compilar bajo `/WX` sin ocultar futuros campos ni inventar información de
escape ajena al propósito de la prueba.

## Validación permitida

Sólo se construye y ejecuta `nsc_precompiled_member_identity_index_tests`. No se ejecuta baseline,
CTest general ni matriz de optimizaciones.

Resultado: 4/4 comprobaciones aprobadas; baseline y suites generales no ejecutados.
