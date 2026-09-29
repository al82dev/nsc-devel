# Tanda 20: cierre de goldens NSMETA 1.20 y 1.21

## Objetivo

Corregir la unica causa raiz del baseline terminado: `nsc.stage48_2.library-metadata`, repetida por los agregadores `advanced-toolchain` y `robustness-final`.

## Diagnostico

La publicacion de funciones libres incremento NSMETA de 1.20 a 1.21 y la publicacion de estado de namespace lo incremento de 1.21 a 1.22. En ambas transiciones se omitio archivar el formato que acababa de convertirse en historico.

Las nuevas tablas superiores tambien agregan sus contadores a la proyeccion ABI. Aunque un modelo no contenga funciones ni estado de namespace, los dos contadores vacios forman parte de la representacion 1.22. Por eso el fingerprint revisado del modelo raiz cambio de `2cc4390f9b25ec1aa54110f0f4187034c648ee13f9a4fbaf2ea9985f15a3bad4` a `4968fe9c9b16c2a0bb8fe9c15a9dd45492f71ac055c6b59748abaffa582c0fa9`.

Los cinco checks reportados eran tres fallos directos y dos cascadas:

- fingerprint esperado obsoleto;
- snapshot 1.20 ausente;
- snapshot 1.21 ausente;
- las dos comprobaciones de compatibilidad no podian ejecutarse porque sus snapshots no se abrieron.

## Cambios

- Se acepta explicitamente el fingerprint correspondiente a la proyeccion NSMETA 1.22 revisada.
- Se archivan los bytes inmutables de NSMETA 1.20 y NSMETA 1.21.
- `NSC_P2_REFRESH_GOLDENS=1` ahora completa snapshots ausentes sin sobrescribir los ya archivados.
- Una sustitucion intencional de evidencia existente requiere tambien `NSC_P2_OVERWRITE_GOLDENS=1`.
- El mensaje de compatibilidad habla de la ventana certificada de productores, porque puede contener el productor N-1 o el productor actual.

## Archivos modificados

- `tests/unit/Stage48_2LibraryMetadataTests.cpp`: actualiza el fingerprint y protege los goldens historicos.

## Archivos nuevos

- `tests/fixtures/nsmeta/historical-1.20.nsmeta`: snapshot binario del formato anterior a funciones de namespace.
- `tests/fixtures/nsmeta/historical-1.21.nsmeta`: snapshot binario del formato con funciones de namespace y anterior al estado de namespace.
- `docs/2.0/development/namespace-functions/TANDA-20-GOLDENS-NSMETA-1.20-1.21.md`: registra causa, cambios, archivos y verificacion.

No se crearon scripts ni directorios de construccion.

## Mejora obtenida

La matriz de compatibilidad vuelve a cubrir todos los formatos NSMETA 1.0-1.21 mediante archivos reales e inmutables. Las futuras ampliaciones pueden generar el snapshot recien historico sin destruir evidencia anterior por accidente.

## Verificacion

- Construccion exclusiva de `nsc` y `nsc_stage48_2_library_metadata_tests`: correcta.
- Generacion controlada: solo aparecieron `historical-1.20.nsmeta` y `historical-1.21.nsmeta`; los snapshots 1.0-1.19 no cambiaron.
- `nsc.stage48_2.library-metadata`: PASS, 302 comprobaciones.
- `nsc.selfhost.advanced-toolchain`: PASS.
- `nsc.selfhost.robustness-final`: PASS.
- No se ejecuto baseline, suite general ni matriz de optimizaciones.
- Los archivos `error*.log` del baseline externo no se modificaron ni se incluyeron.
