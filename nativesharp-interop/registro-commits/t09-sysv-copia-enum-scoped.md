# SysV: campo enum class C++

Una vez importados el tipo y sus enumeradores scoped, la ruta SysV existente
pasó sin cambios adicionales el record trivial con campo `enum class Mode :
int`. La función lo recibe por copia y retorna un record mixto pequeño.

`pruebas-tanda-09-sysv-cpp-copia-enum-scoped/validar.py` comparó Clang 23.1.0
y NativeSharp en ocho combinaciones: estática y DLL O0–O3, con llamadas
directa y por puntero. Resultado:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-y5u3fdvg`.

El productor precompilado todavía falla con `required C enum is unavailable`:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-zgqgl6n6/estatica-O0-producer-ns.log`.
Se conserva `validar-precompilado-pendiente.py` para reproducirlo. No se
acreditan precompilación ni otras bases de enum scoped. T09-003 sigue abierto.
