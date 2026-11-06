# Callback con puntero a record C y biblioteca precompilada

El fixture prepara una biblioteca NativeSharp que publica callbacks C de
firma `int(CallbackPayload*)`, un holder C y un callback leído del global.
Un consumidor con el mismo header C usa los tres caminos. El script prepara
la biblioteca C y la matriz O0–O3 en `A:/nsc-interop-build`.

Esta preparación no demuestra que la firma ni sus dependencias sobrevivan a
la metadata; la validación está pendiente.
