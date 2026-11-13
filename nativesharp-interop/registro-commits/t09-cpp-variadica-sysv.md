# T09: funcion libre C++ variadica SysV, avance parcial

## Cambio

NSC proyecta declaraciones de funciones libres C++ con la convencion `sysv_abi` y parametros variadicos, que antes rechazaba por su linkage C++. Es una dependencia no lineal de T10-003 y un avance parcial de T09-003; ambos elementos siguen abiertos.

## Validacion

- Compilacion incremental de NSC: `build 0`.
- Ocho comparaciones C++/NativeSharp en biblioteca estatica y DLL, de O0 a O3, con argumentos `sbyte`, `float` y `ushort`, conversiones variadicas a `int`, `double` e `int`, comprobacion de registros y seleccion de sobrecarga fija: `A:/temp/nsc-t09-cpp-variadica-sysv-grlo2mt0`.
- Dieciseis consumidores precompilados con y sin encabezado: `A:/temp/nsc-t09-cpp-variadica-sysv-precompilado-wjefmvh8`.
- Regresion C++ cdecl variadica: `A:/temp/nsc-t09-cpp-variadica-cdecl-nq3etf8c`. Rechazo de `preserve_none` variadica: `A:/temp/nsc-t09-cpp-variadica-rechazo-f8zeief1`.
- Regresion de funciones variadicas C SysV, incluyendo rechazo de record NativeSharp y rutas precompiladas: `A:/temp/nsc-t09-sysv-varargs-gg91hc86`.

## Limites

El receptor SysV de la prueba usa ensamblador `naked` para comprobar los registros de esta llamada. En el destino Windows de Clang, la investigacion no encontro las primitivas `__builtin_sysv_va_list` y `__builtin_sysv_va_start`; por ello esta prueba no demuestra ejecucion de `va_arg` SysV C++ ni acredita otras firmas, registros pasados por valor o argumentos en pila. No modifica Clang ni NSLink y no introduce un contrato historico de NSMeta. Los casos sin evidencia permanecen pendientes.