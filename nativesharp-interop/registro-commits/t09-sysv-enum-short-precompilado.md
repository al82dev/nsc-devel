# SysV: enum C++ short precompilado

El productor no encontraba en el catálogo el enum C++ de base `short`. La
captura y el codec vigente conservan ahora su base de 16 bits, signo, identidad
y valores; el plan precompilado coteja la base y disposición del enum
restaurado con el campo y su representación física.

`pruebas-tanda-09-sysv-cpp-copia-enum-short/validar-precompilado.py` pasó las
16 combinaciones de estática y DLL O0–O3, con y sin encabezado y con −32768:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-sjunb8vu`. El control de
catálogo comprobó la conservación del mínimo y la base de 16 bits; pasaron
identidad, ABI y transporte. Pasaron también las 16 combinaciones del enum
sin signo de 32 bits (`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-org2qhth`)
y la regresión C (`A:/temp/nsc-t10-enum-cpp-global-t52hm8zx`).

Se acredita esta firma y un enum global de base `short`; otras bases y formas
siguen pendientes. T09-003 y T10-002 permanecen abiertos.
