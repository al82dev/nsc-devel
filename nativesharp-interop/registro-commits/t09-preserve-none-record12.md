# Registro C++ de doce bytes preserve_none

Se habilita la función `preserve_none` que recibe y devuelve el mismo registro C++ trivial de doce bytes, en la muestra `Small { int x,y,z; }`. Clang 23.1.0 utiliza retorno `sret` y argumento indirecto, alineados a cuatro; la prueba comprueba los tres campos y la copia.

El fixture `pruebas-tanda-09-preserve-none-record12` pasó ocho comparaciones Clang/NativeSharp de llamadas directa y por puntero, en estática y DLL O0–O3 (`A:/temp/nsc-t09-preserve-cpp-record24-7cdox24p`) y dieciséis consumidores precompilados con y sin encabezado (`A:/temp/nsc-t09-preserve-cpp-record24-precompilado-ubif4hqj`). La compilación incremental pasó. La prueba que antes rechazaba doce bytes se conserva y ahora verifica el rechazo del registro aún no implementado de dieciséis bytes (`A:/temp/nsc-t09-preserve-none-record-negativo-d3v5vz4g`).

El resultado no acredita todas las disposiciones de doce bytes, otros tamaños, clases no triviales, variádicas ni TLS aislado. T09-003 y la tanda 9 permanecen abiertos.
