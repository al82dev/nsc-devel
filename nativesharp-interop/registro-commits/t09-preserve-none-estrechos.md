# Enteros estrechos preserve_none

Se admiten llamadas C directas e indirectas `preserve_none` con un parámetro y retorno del mismo tipo: `signed char`, `unsigned char`, `short` o `unsigned short`. La prueba comprueba valores negativos y positivos, anchos LLVM y convención de cada llamada; no generaliza a combinaciones de tipos diferentes.

El fixture `pruebas-tanda-09-preserve-none-estrechos` pasó ocho comparaciones con Clang 23.1.0 en estática y DLL O0–O3 (`A:/temp/nsc-t09-preserve-all-estrechos-vc8_4les`) y dieciséis consumidores precompilados con y sin encabezado (`A:/temp/nsc-t09-preserve-all-estrechos-precompilado-iibly2u5`). La prueba de caché del fixture comprueba una compilación sin entrada y otra con recuperación, para estática y DLL a O0. La prueba negativa conservada rechazó `int(short)` (`A:/temp/nsc-t09-preserve-none-negativo-vsbrks1k`). La compilación incremental pasó.

Otros argumentos y retornos, agregados, variádicas y TLS aislado requieren evidencia propia. T09-003 y la tanda 9 permanecen abiertos.
