# Callback con puntero a record C y biblioteca precompilada

El fixture prepara una biblioteca NativeSharp que publica callbacks C de
firma `int(CallbackPayload*)`, un holder C y un callback leído del global.
Un consumidor con el mismo header C usa los tres caminos. El script prepara
la biblioteca C y la matriz O0–O3 en `A:/nsc-interop-build`.

Esta preparación no demuestra que la firma ni sus dependencias sobrevivan a
la metadata; la validación está pendiente.
El script acepta `--diagnostico` para limitar una reproducción a O0; sin ese
argumento conserva la matriz O0–O3 para la validación agrupada.

Con el build `3f82235fd35e7b9a`, el diagnóstico O0 produjo la biblioteca y su
NSMETA, compiló y enlazó el consumidor y ejecutó los tres accesos con código 0.
O1–O3 y otras firmas permanecen pendientes; esta muestra no cierra T08-017.
