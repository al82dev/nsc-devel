# Enum C++ de base short

Clang ejecutaba un enum global de base `short`, pero NSC rechazaba su campo y
enumerador. Reproducción:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-v_81pdbd/estatica-O0-ns-ir.log`.

El inventario proyecta el enum nombrado con base `short` de 16 bits,
conservando signo, layout e identidad. Sus enumeradores se comprueban contra
la representación observada por Clang.

`pruebas-tanda-10-enum-cpp-short/validar.py` contrastó con Clang 23.1.0 los
valores −32768 y 32767, copia y acceso a campo en O0–O3:
`A:/temp/nsc-t10-enum-cpp-short-qgrsxgsy`. Pasaron las regresiones de C y enum
global de 32 bits (`A:/temp/nsc-t10-enum-cpp-global-6rzetr_7`) y de enum sin
signo (`A:/temp/nsc-t10-enum-cpp-unsigned-b2x_vfys`).

No se acredita aún el paso del record por copia ni la precompilación del enum
short. Otras bases y formas permanecen pendientes; T09-003 y T10-002 siguen
abiertos.
