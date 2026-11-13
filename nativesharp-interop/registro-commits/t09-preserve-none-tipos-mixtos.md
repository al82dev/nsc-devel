# Firmas escalares preserve_none con tipos distintos

Se habilitan llamadas C `preserve_none` directas y por puntero para `int(short)`, `unsigned int(unsigned char)`, `long long(int)` y `double(float)`. El fixture verifica valores negativos, enteros sin signo altos, un entero fuera de 32 bits y valores flotantes, además de la convención en LLVM.

`pruebas-tanda-09-preserve-none-tipos-mixtos` pasó ocho comparaciones Clang 23.1.0/NativeSharp en estática y DLL O0–O3 (`A:/temp/nsc-t09-preserve-all-escalares-retorno-o3dzy09x`) y dieciséis consumidores precompilados con y sin encabezado (`A:/temp/nsc-t09-preserve-all-escalares-retorno-precompilado-fcrhny5f`). En O0, estática y DLL pasaron antes y después de recuperar el caché. La prueba negativa conservada rechaza `int(float)` (`A:/temp/nsc-t09-preserve-none-negativo-u_esvn9x`). La compilación incremental pasó.

Solo se acreditan las cuatro firmas indicadas en Windows x64. Otras combinaciones, agregados, variádicas y TLS aislado requieren evidencia propia; T09-003 y la tanda 9 siguen abiertos.
