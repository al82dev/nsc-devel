# Registros C++ de 20 y 32 bytes preserve_none

Se habilitan funciones `preserve_none` que reciben y devuelven el mismo registro C++ trivial de 20 o 32 bytes. Las muestras contienen cinco y ocho campos `int`, respectivamente. En ambos casos Clang 23.1.0 usa retorno `sret` y argumento indirecto alineados a cuatro; se verifican todos los campos y la copia.

Los fixtures `pruebas-tanda-09-preserve-none-record20` y `pruebas-tanda-09-preserve-none-record32` pasaron, cada uno, ocho comparaciones Clang/NativeSharp directas y por puntero en estática y DLL O0–O3 (`A:/temp/nsc-t09-preserve-cpp-record24-09bgxo87` y `A:/temp/nsc-t09-preserve-cpp-record24-8mzk1wjy`). Cada uno pasó además dieciséis consumidores precompilados con y sin encabezado (`A:/temp/nsc-t09-preserve-cpp-record24-precompilado-qaluwcf6` y `A:/temp/nsc-t09-preserve-cpp-record24-precompilado-vkhuis1_`). La compilación incremental agrupada pasó. La prueba de rechazo se conserva para la forma aún no implementada de 28 bytes (`A:/temp/nsc-t09-preserve-none-record-negativo-dbq7cltj`).

Se acreditan las muestras y el destino Windows x64, no todas las disposiciones de estos tamaños ni otros tamaños. Las muestras posteriores de 28 y 40 bytes se documentan [aquí](t09-preserve-none-record28-40.md). Siguen pendientes las clases no triviales, variádicas y TLS aislado; T09-003 y la tanda 9 permanecen abiertos.
