# NS3 baseline: tipo físico de `Interlocked.Load` self-hosted

## Problema

El backend self-hosted derivaba correctamente el tipo físico de una operación `Interlocked` desde
su primer argumento `ref`, pero después lo sobrescribía con el tipo del argumento situado en la
posición de valor. En `Load(ref long, MemoryOrder)` esa posición contiene el orden de memoria, no un
valor; como el enum se representa como tipo nominal, terminaba emitiendo un `load ptr` y comparándolo
contra `i32 0`.

## Cambio

La inferencia desde el argumento de valor sólo se ejecuta para operaciones que realmente tienen
uno: `Store`, `Add`, `Exchange` y `CompareExchange`. `Load`, `Increment` y `Decrement` conservan el
tipo derivado de la ubicación atómica.

## Archivos

- `selfhost/nsc/CodeGen/LlvmBackend.ns`

## Mejora

`Interlocked.Load(ref long, MemoryOrder)` produce un `load i64` válido. El arreglo también evita que
el orden de memoria contamine el ancho de `Increment` y `Decrement`, manteniendo consistente toda la
familia de intrínsecos self-hosted.
