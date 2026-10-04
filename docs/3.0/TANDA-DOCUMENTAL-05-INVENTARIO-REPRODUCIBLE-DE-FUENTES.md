# Tanda documental 05 — inventario reproducible de fuentes

Fecha: 2026-10-04

Rama: `development`

## Objetivo

Demostrar qué roadmaps, planes, trackers, limitaciones, contratos y expedientes fueron considerados
por el documento maestro, sin convertir ninguno de ellos en una segunda autoridad de estado.

## Implementación

- `tools/native_sharp_3_source_inventory.py` recorre los repositorios activos y las fuentes
  históricas seleccionadas de `A:`.
- En repositorios Git sólo considera Markdown versionado; en archivos históricos sin Git recorre
  Markdown y excluye builds, caches, dependencias y artefactos.
- Cada entrada conserva dueño, ruta absoluta, clase, SHA-256 y disposición normativa.
- El JSON contiene el detalle completo; el Markdown raíz contiene los totales por dueño y clase.
- `verify_native_sharp_3_catalog.py` regenera ambos resultados en memoria y exige igualdad byte a
  byte. Una fuente nueva, eliminada o modificada obliga a regenerar y reconciliar el inventario.

## Productos

- `A:\NATIVESHARP-3.0-SOURCE-INVENTORY.json`: inventario completo y determinista.
- `A:\NATIVESHARP-3.0-SOURCE-REGISTRY.md`: resumen legible subordinado a §24.
- `tools/native_sharp_3_source_inventory.py`: generador y modo `--check`.
- `tools/verify_native_sharp_3_catalog.py`: gate integrado.
- `tests/unit/test_native_sharp_3_catalog.py`: prueba de serialización determinista y subordinación.

## Límite

El inventario prueba cobertura documental y detecta drift. No certifica features ni sustituye las
pruebas dirigidas de cada componente. No se ejecuta baseline ni CTest general.
