# SysV: campo enum class precompilado

El productor no encontraba el `enum class` en el catálogo porque la captura
excluía el ámbito scoped. El contrato vigente conserva ahora esa propiedad
junto con el origen C++, USR, base y valores; el consumidor reconstruye los
enumeradores como miembros del enum, sin publicarlos en el namespace exterior.
La codificación previa de enums C y C++ no scoped permanece igual, sin una
selección de contratos basada en etiquetas históricas.

`pruebas-tanda-09-sysv-cpp-copia-enum-scoped/validar-precompilado.py` pasó las
16 combinaciones de estática y DLL O0–O3, con y sin encabezado:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-4c0cbfoz`. El control
del catálogo comprobó la restauración de scoped y de su valor; también pasaron
identidad, ABI y transporte. Pasaron las 16 combinaciones previas de enum sin
signo (`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-m402f_3x`) y la
regresión C (`A:/temp/nsc-t10-enum-cpp-global-ajwwwsmk`).

La evidencia cubre un enum class global de base `int` en un record y una
firma concretos; no acredita otras bases ni interoperabilidad general.
T09-003 y T10-002 siguen abiertos.
