# Tanda 4: snapshots, cancelación y recuperación semántica

## Objetivo

Producir resultados inmutables por generación y conservar el último modelo semántico válido cuando
el documento abierto queda temporalmente incompleto.

## Diagnóstico

La compilación de proyecto es deliberadamente de un solo uso. Un editor, en cambio, recibe cambios
continuos y puede solicitar información mientras existe una comilla o llave sin cerrar. Reutilizar
un objeto mutable mezclaría versiones; eliminar el modelo en cada error haría parpadear IntelliSense.

## Cambios

- Cada análisis crea una `ProjectCompilation` nueva y un snapshot con generación monotónica.
- Los overlays se copian al comenzar, de modo que el análisis observa una versión coherente.
- El pipeline se detiene en cuerpos semánticos; no construye HIR, MIR, LLVM ni artefactos.
- La cancelación existente de `ProjectCompilation` se expone al servicio.
- Los diagnósticos siempre pertenecen al intento actual.
- Si el intento falla o se cancela, el snapshot puede conservar por separado el último modelo
  semántico válido del mismo proyecto.

## Archivos modificados

- `include/nativesharp/LanguageService/LanguageService.h`.
- `src/LanguageService/LanguageService.cpp`.
- `CMakeLists.txt`.

## Archivos nuevos

- `tests/unit/LanguageServiceSnapshotTests.cpp`.
- `docs/lsp/TANDA-04-SNAPSHOTS-Y-RECUPERACION.md`.

## Mejora obtenida

El servidor podrá publicar inmediatamente el error del texto actual sin perder navegación y hover
del último estado correcto. Las respuestas viejas se distinguen por generación y pueden descartarse.

## Verificación

- Construcción y ejecución exclusivas de `nsc_language_service_snapshot_tests`.
- Proyecto válido, overlay inválido, fallback semántico y cancelación inmediata.
- Construcción exclusiva de `nsc`.
- No se ejecutó baseline ni CTest general.
