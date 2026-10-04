# Tanda correctiva — Stage 46 y el fixture NSMETA 1.10

Fecha: 2026-10-04

Rama: `development`

Estado: terminada

## Problema

`nsc.stage46.native-libraries` fallaba en tres comprobaciones relacionadas con una clase
`static` producida históricamente con una jerarquía inválida. El primer fallo impedía escribir
`LegacyStaticHierarchy/NativeMath.nsmeta`; la verificación del artefacto y la compilación del
consumidor fallaban después por la ausencia de ese mismo archivo.

El modelo usado por el fixture provenía del productor actual. Desde NSMETA 1.35, sus miembros y
funciones incluyen resúmenes interprocedurales de escape. El fixture cambiaba únicamente el minor
a 1.10, por lo que `LibraryMetadataCodec::writeModel` lo rechazaba correctamente: NSMETA 1.10 no
puede representar esos resúmenes.

Además, la prueba componía el texto del diagnóstico en el mismo llamado a `tests.expect` que
ejecutaba `writeModel`. El orden de evaluación de los argumentos permitía capturar `error` antes
de que el codec lo escribiera, de modo que el log mostraba paréntesis vacíos.

## Cambios

- El fixture elimina `parameterEscapeSummary` de miembros y funciones antes de serializar el
  modelo histórico 1.10, siguiendo el mismo contrato de downgrade que ya prueban los gates de
  compatibilidad NSMETA.
- El resultado de `writeModel` se evalúa y guarda antes de construir el diagnóstico. Si vuelve a
  fallar, Stage 46 conserva ahora la causa exacta en el log.

## Archivos modificados

- `tests/unit/Stage46NativeLibrariesTests.cpp`

## Archivos nuevos

- `docs/compiler-optimization/TANDA-CORRECTIVA-STAGE46-NSMETA-LEGACY.md`: registra la causa, el
  alcance y la validación de esta corrección.

## Mejora

La prueba histórica vuelve a medir lo que declara: que el lector actual ignora la jerarquía
espuria de una clase `static` escrita por un productor NSMETA 1.10. Ya no intenta introducir en
ese archivo un contrato de análisis creado veinticinco versiones menores después.

No se relajó el codec, no se modificó el formato NSMETA actual y no se alteró la resolución de
`LibraryReference`.

## Validación dirigida

- Se reconstruyó únicamente `nsc_stage46_native_libraries_tests`.
- Se ejecutó únicamente `nsc.stage46.native-libraries` con el NSLink del staging
  `A:\native-sharp-3.0-baseline`.
- Resultado: `PASS`, 456/456 comprobaciones, 57.6 segundos.
- No se ejecutó baseline ni una suite general.
