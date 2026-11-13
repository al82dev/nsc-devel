# T09: enum class y record en una llamada variádica SysV

Se verificó una llamada C++ Windows x64 con `enum class` de base `unsigned short` y un record trivial `{double,int}` como argumentos opcionales. El receptor comprueba registros y la prueba comprueba que el record original no cambia. No se cambió el compilador.

- El IR O0 de Clang, `A:/temp/t09-sysv-enum-record-control.ll`, presenta `i16 zeroext`, `double` e `i32`. Los validadores comprueban las mismas piezas en el IR de NSC.
- Ocho comparaciones Clang/NativeSharp por ruta, estática y DLL O0–O3: `A:/temp/nsc-t09-cpp-variadica-sysv-enum-record-8es85lad` y `A:/temp/nsc-t09-cpp-variadica-sysv-enum-record-indirecta-d76hwuw_`.
- Dieciséis consumidores precompilados por ruta, con y sin encabezado: `A:/temp/nsc-t09-cpp-variadica-sysv-enum-record-precompilado-c293ds3r` y `A:/temp/nsc-t09-cpp-variadica-sysv-enum-record-indirecta-precompilado-169jvhq9`.

Esta evidencia cubre una firma y un layout; no acredita todas las combinaciones ni `va_arg` SysV en Windows. Los ítems generales T09-003, T10-002 y T10-003 siguen abiertos.
