# Tanda 17: correccion de contratos detectada por baseline

## Objetivo

Corregir la desincronizacion de versiones que el baseline detecto despues de completar el estado de namespace, sin volver a ejecutar la suite general.

## Diagnostico

Los fallos de layout, ownership, excepciones y genericos no eran regresiones de esas areas. Los tests exigian literalmente MIR 1.11 aunque el compilador ya publicaba MIR 1.13. La tanda 16 agrego nodos y semantica observables al MIR, por lo que correspondia incrementar el esquema nuevamente. Tambien quedaron contratos de distribucion que seguian anunciando NSMETA 1.20 aunque el formato actual es 1.22.

## Cambios

- El esquema MIR sube de 1.13 a 1.14 por los nodos NamespaceStateLoad y NamespaceStateAddress y la identidad estable de almacenamiento.
- Las pruebas que verifican el volcado MIR esperan deliberadamente 1.14.
- El contrato del toolchain y el baseline de migracion esperan MIR 1.14 y NSMETA 1.22.
- La matriz de compatibilidad publica NSMETA N=1.22 y mantiene N-1=1.20 como productor aceptado.
- La prueba vectorcall conserva su prueba de rechazo anterior a NSMETA 1.20, pero valida que el productor actual escriba 1.22.

## Archivos modificados

- include/nativesharp/Frontend/ProjectOopMirModel.h
- src/Driver/CompatibilityPolicy.cpp
- tests/unit/IntegratedAssemblyMirTests.cpp
- tests/unit/Item322NoHeapPropagationTests.cpp
- tests/unit/Item323StackClassEscapeAnalysisTests.cpp
- tests/unit/Item329BitFieldAccessTests.cpp
- tests/unit/Item344VectorCallTests.cpp
- tests/unit/Item395MirLayoutAbiTests.cpp
- tests/unit/Item396MirOwnershipCleanupTests.cpp
- tests/unit/Item397MirExceptionFlowTests.cpp
- tests/unit/Item398MirGenericsInterfacesTests.cpp
- tests/unit/Item405FinalCompatibilityCertificationTests.cpp
- tests/unit/Stage62ToolchainContractTests.cpp
- tests/unit/Stage69MigrationBaselineTests.cpp

## Archivo nuevo

- docs/2.0/development/namespace-functions/TANDA-17-CORRECCION-CONTRATOS-BASELINE.md: documenta la causa, el alcance y la verificacion de la reparacion.

## Mejora obtenida

Los consumidores del volcado MIR pueden rechazar de forma correcta una semantica que no entienden, y los artefactos de distribucion vuelven a describir las versiones que el compilador realmente produce. Las pruebas ya no confunden un cambio de contrato deliberado con regresiones de layout, cleanup, SEH o genericos.

## Verificacion

- Construccion incremental exclusiva de nsc: correcta.
- Los 12 tests afectados se construyeron y ejecutaron individualmente: PASS, 198 comprobaciones.
- nsc_namespace_state_model_tests: PASS, 24 comprobaciones.
- No se ejecuto baseline, CTest general ni matriz de optimizaciones.
- Los error*.log generados por el baseline cancelado no se modificaron ni se incluyeron.
