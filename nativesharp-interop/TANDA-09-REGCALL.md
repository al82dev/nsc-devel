# T09-003: regcall escalar directo en Windows x64

Clang oficial 23.1.0 emite `x86_regcallcc` y el simbolo
`__regcall3__t09_regcall` para el control de tres argumentos `int` y retorno
`int` (`A:/temp/t09-convention-oracle.ll`). El control C independiente paso
estatica y DLL en O0-O3 (`A:/temp/nsc-t09-regcall-y71k72p2`), mientras NSC
rechazaba la convencion (`A:/temp/nsc-t09-regcall-wixfx9br`).

La sesion obtiene el simbolo de CodeGen de Clang; HIR y MIR transportan la
convencion e incluyen el dato en identidades y serializacion de cache. LLVM
aplica `x86_regcallcc` a declaracion y llamada. El control comprueba simbolo
en el objeto, llamada LLVM, ejecucion con tres valores diferentes y cache
miss/hit en estatica y DLL. Pasaron las ocho comparaciones C/NativeSharp O0-O3
(`A:/temp/nsc-t09-regcall-pvsd9fxk`) y 16 consumidores de plantilla con y
sin encabezado (`A:/temp/nsc-t09-regcall-precompilado-ndl5ojoc`).

Una firma con argumento `double` se rechaza
(`A:/temp/nsc-t09-regcall-negativo-9ykay9qz`); no se acreditan otros tipos,
records, punteros a funcion, DLL con aislamiento TLS ni otras convenciones.
Pasaron regresiones cdecl (`A:/temp/nsc-t09-llamadas-67a6q_9a`), vectorcall
(`A:/temp/nsc-t09-vectorcall-k8y58pzj`), SysV precompilado
(`A:/temp/nsc-t09-sysv-indirecta-opcional-flotante-pre-5_ukljdt`), cuatro
sondas (`A:/temp/t09-test-varptr.log`) y transporte/ABI/identidad
(`A:/temp/t09-transport-directed/`). T09-003 y la tanda permanecen abiertos.

## Enteros de distintas anchuras

Una muestra con `unsigned int`, `long long` y `unsigned long long` de
parametros y retorno `unsigned long long` conserva valores negativos y el bit
alto. El control C paso estatica/DLL O0-O3
(`A:/temp/nsc-t09-regcall-enteros-24onjjoy`) y NSC rechazo inicialmente la
firma (`A:/temp/nsc-t09-regcall-enteros-4u22keva`). Tras ampliar el filtro a
enteros de 32 y 64 bits con y sin signo, pasaron ocho comparaciones C/NS,
IR, simbolo y cache (`A:/temp/nsc-t09-regcall-enteros-zwvfgqyn`) y 16
consumidores precompilados con y sin encabezado
(`A:/temp/nsc-t09-regcall-enteros-precompilado-nr9qolj4`). Pasaron la muestra
anterior (`A:/temp/nsc-t09-regcall-bwl9lztu`), rechazo de `double`
(`A:/temp/nsc-t09-regcall-negativo-eqeddmfj`), cuatro sondas
(`A:/temp/t09-test-varptr.log`) y transporte/ABI/identidad
(`A:/temp/t09-transport-directed/`). La muestra no acredita todas las firmas;
T09-003 permanece abierto.
