# Enums C++ signed char y unsigned char en SysV

Clang ejecutaba los records con enums de un byte, pero NSC rechazaba sus
enumeradores y campos. La importación, el plan físico y el catálogo vigente
conservan ahora la identidad y el signo de `signed char` y `unsigned char`.
El plan compara el campo con `i8`; no se admite `char` sin signo explícito
por este cambio.

Para `signed char`, la prueba de tipo y valores extremos pasó en O0–O3
(`A:/temp/nsc-t10-enum-cpp-schar-ruwbkgrs`); la llamada SysV pasó ocho
comparaciones (`A:/temp/nsc-t09-sysv-cpp-copia-relleno-b45msdg8`) y 16
consumidores precompilados (`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-kxsclg25`).

Para `unsigned char`, la prueba de tipo y extremos pasó en O0–O3
(`A:/temp/nsc-t10-enum-cpp-uchar-muim930q`); la llamada pasó ocho
comparaciones (`A:/temp/nsc-t09-sysv-cpp-copia-relleno-n0fepnx1`) y 16
consumidores precompilados (`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-dtdf99or`).
Los controles usaron Clang 23.1.0, estática y DLL, con y sin encabezado en
consumo precompilado.

Pasaron los controles de identidad, ABI y transporte, y la regresión C
(`A:/temp/nsc-t10-enum-cpp-global-zx4ecw3t`). Los controles negativos ahora
usan `char` sin signo explícito, cuya proyección no se acredita aquí.

La evidencia SysV corresponde a enums globales no scoped y una firma
concreta. Otras formas siguen pendientes; T09-003 y T10-002 permanecen abiertos.
