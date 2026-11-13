# Callback booleano preserve_none

Se habilita la llamada indirecta C `preserve_none` de firma `_Bool(_Bool)`, que ya se admitía directamente. El fixture `pruebas-tanda-09-preserve-none-bool` verifica verdadero y falso, igualdad de punteros y `zeroext` del argumento y retorno en LLVM.

Pasaron ocho comparaciones Clang 23.1.0/NativeSharp para estática y DLL O0–O3 (`A:/temp/nsc-t09-preserve-bool-6jkehokk`) y dieciséis consumidores precompilados con y sin encabezado (`A:/temp/nsc-t09-preserve-bool-precompilado-qo5tew0f`). Se observó la primera compilación sin entrada de caché y la segunda con recuperación para estática y DLL en O0; los ejecutables resultantes pasaron. La compilación incremental terminó correctamente.

La evidencia se limita a esta firma y al destino Windows x64; otras combinaciones de tipos, agregados, variádicas y TLS aislado permanecen pendientes. T09-003 y la tanda 9 siguen abiertos.
