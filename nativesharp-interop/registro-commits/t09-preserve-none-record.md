# Registro C++ pequeño preserve_none

Se admite una función `preserve_none` que recibe y devuelve por valor el mismo registro C++ trivial de ocho bytes. Se cotejan la identidad y el tamaño de los tipos en la función importada y el tamaño en la llamada indirecta. El caso contrastado es `Small { int x,y; }`; Clang 23.1.0 lo representa como `i64(i64)`.

El fixture `pruebas-tanda-09-preserve-none-record` pasó ocho comparaciones Clang/NativeSharp de llamada directa y por puntero, en estática y DLL O0–O3 (`A:/temp/nsc-t09-preserve-cpp-record-46a_8m33`), y dieciséis consumidores precompilados con y sin encabezado (`A:/temp/nsc-t09-preserve-cpp-record-precompilado-ayqucrtz`). Una prueba negativa comprueba que Clang acepta y NSC rechaza el registro distinto de doce bytes aún no admitido (`A:/temp/nsc-t09-preserve-none-record-negativo-69zzu0je`). La compilación incremental pasó.

No se acreditan todas las formas o identidades de registros de ocho bytes, clases no triviales, variádicas ni TLS aislado. Una muestra posterior de 24 bytes se documenta [aquí](t09-preserve-none-record24.md); otros tamaños siguen pendientes. T09-003 y la tanda 9 continúan abiertos.
