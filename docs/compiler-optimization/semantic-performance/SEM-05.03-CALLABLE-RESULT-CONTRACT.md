# SEM-05.03 — Resultado exclusivo por callable

## Propósito

Definir la unidad que un worker puede producir sin publicar estado parcial en el modelo global.

## Contrato

`ProjectCallableAnalysisResult` es movible y no copiable. Conserva:

- ordinal estable del callable y de su oleada;
- ownership exclusivo de un shard `ProjectMethodBodyModel`;
- `DiagnosticEngine` local;
- ordinales de callables descubiertos.

`normalizeDiscoveredCallables` ordena y deduplica los descubrimientos antes de entregarlos al
coordinador. Ningún orden de finalización de workers participa en el resultado.

## Archivos

- `ProjectCallableAnalysisResult.h/.cpp`;
- `ProjectCallableAnalysisResultTests.cpp`;
- `CMakeLists.txt`;
- este documento.

## Validación dirigida

Build y ejecución individual de `nsc_project_callable_analysis_result_tests`. La prueba valida
identidad, diagnóstico local, orden/deduplicación y transferencia exclusiva del shard. No se
ejecuta baseline, minibaseline ni CTest general.
