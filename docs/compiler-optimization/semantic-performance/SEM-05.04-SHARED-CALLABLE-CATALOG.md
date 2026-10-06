# SEM-05.04 — Catálogo compartido de funciones libres

## Propósito

Evitar que cada shard por callable copie los adapters de funciones libres que ya fueron
construidos y congelados por el modelo raíz.

## Cambio

`ProjectMethodBodyModel` conserva ownership del vector `namespaceCallableAdapters_` y publica
internamente una vista `std::span<const ClassMemberSymbol>`. Todas las lecturas usan la vista;
la construcción sigue ocurriendo una sola vez sobre el vector propietario.

El span se fija después de terminar el catálogo, cuando ya no habrá realocaciones. Los shards
podrán apuntar a esa misma vista inmutable durante una oleada.

## Archivos

- `ProjectMethodBodyModel.h/.cpp`;
- este documento.

## Mejora

- cero copias del catálogo por callable;
- una sola identidad de símbolos para modelo raíz y shards;
- frontera explícita entre productor mutable inicial y consumidores inmutables.

## Validación dirigida

Build y ejecución individual de `nsc_project_method_body_model_tests` en O0. No baseline,
minibaseline, CTest general ni O1/O2/O3.
