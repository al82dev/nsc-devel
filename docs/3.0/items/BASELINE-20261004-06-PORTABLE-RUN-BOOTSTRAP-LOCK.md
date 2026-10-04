# Baseline 2026-10-04: lock de bootstrap portable por corrida

## Problema

NSCBuilder creaba correctamente un `bootstrap.lock` dentro de la evidencia de cada baseline, pero
sólo sustituía las identidades de `nsc0` y NSLink. Python, CMake y Ninja conservaban los hashes y
versiones de la máquina donde se promovió el lock canónico. Una corrida íntegra en otra máquina
podía pasar 609 de 610 pruebas y fallar SH53 aunque sus herramientas hubieran permanecido estables
durante toda la ejecución.

## Cambio

- El lock canónico del repositorio permanece inalterado y sigue describiendo el último entorno
  promovido.
- El lock de evidencia de una corrida congela los ocho artefactos realmente seleccionados:
  `nsc0`, Clang, LLD, `LLVM-C.lib`, NSLink, Python, CMake y Ninja.
- Para ejecutables con consulta de versión se guarda también la primera línea devuelta por el
  binario capturado.
- CMake y Ninja se resuelven desde `PATH`; no se introducen rutas hardcodeadas.
- SH53 continúa comprobando ruta, SHA-256 y versión al final de la corrida, pero contra la foto
  tomada al inicio en esa misma máquina.

## Archivos

- `nscbuilder.py`
- `tests/selfhost/test_bootstrap_infrastructure.py`
- `docs/3.0/items/BASELINE-20261004-06-PORTABLE-RUN-BOOTSTRAP-LOCK.md`

## Mejora

El baseline conserva detección estricta de sustituciones durante su ejecución y puede certificarse
en otra máquina con un toolchain seleccionado distinto. No se relajan hashes, no se modifica el
lock histórico y no se asume la ubicación de CMake o Ninja.

## Validación dirigida

- Casos unitarios exactos del escritor de lock por corrida y de captura de versión.
- No se ejecutó baseline ni CTest general.
