# Tanda universal 32: compatibilidad de ordenamiento estable

## Problema confirmado

`nsc.selfhost.compiler-host-workload` usa `Algorithm.StableSort<T>` para ordenar rutas y para
comprobar que elementos con la misma clave preservan su orden de descubrimiento. La sincronización
de la biblioteca estándar eliminó ese método, pero conservó el workload y el contrato observable.

Sustituir la llamada por `Algorithm.Sort<T>` no es válido: la implementación actual usa heapsort y
no garantiza estabilidad.

## Cambio

Se restaura `StableSort<T>(Span<T>, Comparison<T>)` mediante insertion sort iterativo:

- no usa recursión;
- no reserva memoria auxiliar;
- sólo desplaza un predecesor cuando el comparador devuelve un valor estrictamente negativo;
- preserva el orden relativo de los empates;
- usa `Span.UncheckedAt` y `Span.UncheckedSet`, que son la superficie vigente.

La complejidad es O(n^2) en tiempo y O(1) en memoria. El algoritmo se conserva para tablas pequeñas
del compilador y tooling; no sustituye al `Sort` general.

## Archivos

- Modificado: `lib/NativeSharp.Core.Collections.Generic/Algorithm.ns`.
- Nuevo: este documento.

## Validación requerida

1. Ejecutar `nsc.selfhost.compiler-host-workload` en O0 con NSLink.
2. Reconstruir el árbol de pruebas si CMake detecta el cambio de fuente.
3. Ejecutar la batería CTest completa existente en O0 con NSLink.

