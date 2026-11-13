# Registro C++ de 24 bytes preserve_none

Se amplía la función `preserve_none` que recibe y devuelve el mismo registro C++ trivial a la forma de 24 bytes. El caso comprobado tiene seis enteros y alineación de cuatro bytes. Clang 23.1.0 usa un retorno indirecto `sret` y un argumento indirecto; el fixture comprueba copia y todos sus campos.

`pruebas-tanda-09-preserve-none-record24` pasó ocho comparaciones Clang/NativeSharp, directas y por puntero, en estática y DLL O0–O3 (`A:/temp/nsc-t09-preserve-cpp-record24-77eua2tp`), y dieciséis consumidores precompilados con y sin encabezado (`A:/temp/nsc-t09-preserve-cpp-record24-precompilado-1bp5ae4b`). Se mantiene la prueba negativa que rechaza el registro no implementado de doce bytes (`A:/temp/nsc-t09-preserve-none-record-negativo-lq4h8vjg`). La compilación incremental terminó correctamente.

Esto acredita la muestra y el destino Windows x64, no todas las disposiciones de 24 bytes ni otros tamaños. El caso posterior de doce bytes se documenta [aquí](t09-preserve-none-record12.md). Las clases no triviales, variádicas y TLS aislado siguen pendientes; T09-003 y la tanda 9 permanecen abiertos.
