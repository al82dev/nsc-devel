# SysV: enum C++ unsigned int precompilado

El productor de la muestra no encontraba el enum `unsigned int` al construir
su catálogo, aunque la llamada viva ya funcionaba. La captura, el codec
vigente y la conciliación del plan conservan ahora la base sin signo de 32
bits, el USR, el valor y el layout. El tipo restaurado debe coincidir con el
enum del campo antes de aceptar el plan físico.

`pruebas-tanda-09-sysv-cpp-copia-enum-unsigned/validar-precompilado.py` pasó
las 16 combinaciones de estática y DLL O0–O3, con y sin encabezado y con
`0xFFFFFFFF`: `A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-prvp5myq`.
El control de catálogo verificó la base sin signo y su valor máximo; pasaron
también los controles de identidad, ABI y transporte. La regresión con enum
`int` en namespace pasó sus 16 combinaciones
(`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-kkxsgs_5`) y pasó la
regresión C (`A:/temp/nsc-t10-enum-cpp-global-fq1y6rpn`).

Se acredita esta firma concreta y un enum global no scoped de base
`unsigned int`. Otras bases, enums scoped y demás formas de ABI permanecen
pendientes; T09-003 y T10-002 siguen abiertos.
