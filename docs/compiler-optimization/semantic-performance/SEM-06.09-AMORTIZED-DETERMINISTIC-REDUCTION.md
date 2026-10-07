# SEM-06.09 — Reducción determinista con crecimiento amortizado

## Causa raíz

El probe de 1024 callables separó el costo de la oleada:

- análisis `j1`: 26,165 us;
- análisis `j12`: 5,947 us;
- merge `j1`: 637,196 us;
- merge `j12`: 737,239 us.

El análisis aislado sí escalaba 4.4 veces, pero el merge ocultaba la mejora. Cada resultado pedía
`reserve(destination.size() + source.size())` antes de añadir sus elementos. Como la capacidad
quedaba ajustada al tamaño siguiente, el resultado posterior realocaba y movía otra vez todo el
prefijo. Repetido para N shards y para cada vector semántico, el costo era O(N²).

## Corrección

El merger deja que `std::vector` aplique su crecimiento geométrico amortizado. El orden de
publicación no cambia: los resultados siguen ordenados por `(waveOrdinal, callableOrdinal)` y cada
elemento se añade en ese orden estable. También se elimina la reserva exacta equivalente de
`scopeNames_`.

## Contratos

- complejidad de crecimiento acumulada: O(E), donde E es el total de elementos publicados;
- ownership: cada elemento continúa moviéndose una sola vez desde su shard;
- determinismo: no cambia el orden de reducción ni la construcción de índices;
- concurrencia: el coordinador sigue siendo el único escritor del modelo global;
- recursión: no se introduce ninguna ruta recursiva.

## Archivos

- `src/Frontend/ProjectCallableResultMerger.cpp`;
- este documento.

## Validación requerida

Se reconstruyen y ejecutan únicamente reducer, method bodies, escape summaries y el probe O0.
La aceptación funcional exige resultados idénticos; la medición debe demostrar que el merge deja
de dominar por crecimiento cuadrático. No se ejecutan baseline, minibaseline ni CTest general.

## Resultado medido

En el mismo corpus de 1024 callables, tres repeticiones y mediana:

| Configuración | Method bodies antes | Method bodies después | Analysis después | Merge después |
|---|---:|---:|---:|---:|
| `j1` | 666,370 us | 42,334 us | 26,933 us | 13,158 us |
| `j2` | 682,660 us | 27,548 us | 13,650 us | 9,342 us |
| `j12` | 749,556 us | 19,145 us | 5,720 us | 8,467 us |

El merge `j1` pasó de 637,196 a 13,158 us, una reducción de 97.9 %. El análisis paralelo conserva
su escalamiento y la fase completa mejora 54.8 % entre `j1` y `j12`. La pared del Driver pasa de
112 a 89 ms; las fases anteriores y la carga del proyecto explican la parte no paralela restante.

Validación funcional posterior:

- escape summaries: 8/8;
- reducer: 5/5;
- method bodies: 76/76.
- clang-tidy dirigido del merger: limpio.

La evidencia reproducible se conserva en
`_nscbuilder/semantic-performance/SEM-06.07/callable-wave-performance.log`.
