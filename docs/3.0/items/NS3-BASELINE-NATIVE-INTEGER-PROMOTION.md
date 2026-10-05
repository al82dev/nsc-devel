# NS3 baseline: promociones entre enteros nativos y fijos

## Problema

El clasificador de conversiones comparaba ancho y signo antes de consultar la tabla de aristas
nombradas. En Windows x64 esto hacía que `uint` pareciera ensanchar a `nint`, mientras que las
conversiones explícitamente admitidas `nint -> long` y `nuint -> ulong` quedaban ocultas por un
retorno anticipado al comparar tipos del mismo ancho.

La tabla de 91 pares detectó seis respuestas incorrectas, incluidas las combinaciones simétricas.

## Cambio

Las conversiones nombradas se consultan primero. Si el destino es `nint` o `nuint`, una conversión
no escrita en esa tabla no se infiere solamente porque el objetivo actual sea x64. Las conversiones
fijas restantes conservan la comparación ordinaria por ancho y signo.

## Archivos

- `src/Frontend/ProjectTypeConversion.cpp`

## Mejora

`uint + nint` obtiene `long`; `long + nint` conserva `long`; y `ulong + nuint` conserva `ulong`, en
ambos órdenes, sin convertir silenciosamente un tipo fijo en uno dependiente del ancho nativo.
