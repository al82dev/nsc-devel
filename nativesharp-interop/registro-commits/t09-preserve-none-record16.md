# Registro C++ de dieciséis bytes preserve_none

Se habilita la función `preserve_none` que recibe y devuelve el mismo registro C++ trivial de dieciséis bytes, en la muestra `Small { int x,y,z,w; }`. Clang 23.1.0 usa retorno `sret` y argumento indirecto alineados a cuatro; la prueba verifica los cuatro campos y la copia.

El fixture `pruebas-tanda-09-preserve-none-record16` pasó ocho comparaciones Clang/NativeSharp, directas y por puntero, en estática y DLL O0–O3 (`A:/temp/nsc-t09-preserve-cpp-record24-6wvwzb9u`) y dieciséis consumidores precompilados con y sin encabezado (`A:/temp/nsc-t09-preserve-cpp-record24-precompilado-37a2po3e`). La compilación incremental pasó. La prueba de rechazo se conserva y ahora comprueba veinte bytes, forma aún no implementada (`A:/temp/nsc-t09-preserve-none-record-negativo-ps2lfaix`).

El resultado no acredita todas las disposiciones de dieciséis bytes, otros tamaños, clases no triviales, variádicas ni TLS aislado. T09-003 y la tanda 9 permanecen abiertos.
