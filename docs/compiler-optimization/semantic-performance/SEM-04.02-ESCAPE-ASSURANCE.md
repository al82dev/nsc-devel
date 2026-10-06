# SEM-04.02 - Aseguramiento del worklist de escape

## Contadores

`ProjectEscapePropagationStatistics` registra celdas, aristas, orígenes clasificados, evaluaciones
y aumentos reales de resumen. Dos contadores contractuales deben permanecer en cero:

- `legacyGlobalCallRescans`;
- `recursiveVariableArrayFallbacks`.

El recorrido para encontrar la raíz de un VLA ahora usa una pila explícita en heap y conserva el
orden `first`, `second`, `third`, `children`.

## Prueba dirigida

La suite Stage 50.7 ya incluye forward de borrowed returns y llamadas. Ahora comprueba que el
grafo contiene celdas/aristas, que el worklist fue evaluado y que no reaparecieron las dos rutas
prohibidas.

## Alcance

Los contadores son estructurales, no temporales; no se imprimen en compilación normal ni afectan
artefactos. Sólo se ejecutan los targets individuales Stage 50.7 y MethodBody en O0.
