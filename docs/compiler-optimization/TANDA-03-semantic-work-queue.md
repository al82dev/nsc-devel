# Tanda 3: cola de trabajo semántica

## Objetivo

Evitar que el análisis de cuerpos vuelva a recorrer todos los miembros y todas las llamadas cada vez que descubre una nueva especialización genérica necesaria.

## Cambios realizados

- Se reemplazó el ciclo de rescaneo global por una cola explícita de miembros invocables.
- Se añadieron cursores independientes para procesar únicamente llamadas y conversiones a delegado recién descubiertas.
- Las nuevas demandas genéricas se deduplican y ordenan antes de agregarse a la siguiente ola de trabajo.
- Se conservó el orden determinista del análisis.

## Archivos modificados

- `src/Frontend/ProjectMethodBodyModel.cpp`

## Archivos nuevos

- Este informe. La cola usa las estructuras del analizador existente y no requiere un componente nuevo.

## Mejora esperada

El costo deja de crecer por rescaneos completos de miembros, llamadas y delegados acumulados. En proyectos con muchas especializaciones genéricas, cada elemento nuevo se consume una sola vez dentro de la cola incremental.

## Verificación de la tanda

- Construcción de `nsc` en configuración Debug/O0: correcta.
- Prueba dirigida `nsc.project-method-body-model`: correcta.
- Prueba dirigida `nsc.stage59.closed-generic-member`: correcta.
- No se ejecutó baseline completo.
