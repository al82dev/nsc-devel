# Tanda universal 33: compatibilidad de métricas de ArenaAllocator

## Problema confirmado

Después de restaurar `Algorithm.StableSort<T>`, el workload del compilador avanzó hasta análisis de
cuerpos y expuso cinco miembros públicos ausentes en `ArenaAllocator`:

- `TotalAllocationCount`;
- `ActiveAllocationCount`;
- `ResetCount`;
- `HasOutstandingAllocations`;
- el estado interno necesario para mantener esas métricas.

El commit de sincronización de la stdlib eliminó simultáneamente los campos, sus actualizaciones y
los accessors, aunque el workload conservó el contrato.

## Cambio

Se restauran los tres contadores y sus invariantes:

- toda asignación incrementa el total histórico y el total activo;
- `Reset` lleva las asignaciones activas a cero e incrementa el conteo de reinicios;
- los accessors sólo observan el estado y no cambian la política de asignación de la arena.

No se modifica el compilador ni se debilita el workload.

## Archivos

- Modificado: `lib/NativeSharp.Core/ArenaAllocator.ns`.
- Nuevo: este documento.

## Validación requerida

1. Ejecutar `nsc.selfhost.compiler-host-workload` en O0 con NSLink.
2. Reconstruir el árbol de pruebas.
3. Ejecutar la batería CTest completa existente en O0 con NSLink.

