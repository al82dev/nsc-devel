# SysV: record con campo enum C++ en namespace

El record C++ trivial con `T09Cpp::Mode` ya exponía su campo, pero la llamada
SysV por puntero aún se rechazaba. Reproducción:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-p8b6sls7/estatica-O0-ns-ir.log`.

La ruta viva concilia el nombre completo y USR del enum con el tipo instalado,
su base y disposición, antes de comprobar el `i32` del plan de Clang. Los
controles del plan comprueban el rechazo de un USR o nombre ausente y de una
base distinta. La representación física sola no acredita identidad.

`pruebas-tanda-09-sysv-cpp-copia-enum-namespace/validar.py` pasó las ocho
comparaciones con Clang 23.1.0, estática y DLL O0–O3, para llamada directa y
por puntero: `A:/temp/nsc-t09-sysv-cpp-copia-relleno-76u04211`.
`nsc.cpp-interop.regcall-record-plan` también pasó.

El productor precompilado todavía falla al localizar el enum en el catálogo:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-czpghi82/estatica-O0-producer-ns.log`.
Se conserva `validar-precompilado-pendiente.py` como reproducción. La evidencia
no cubre otras bases ni enums scoped. T09-003 sigue abierto.
