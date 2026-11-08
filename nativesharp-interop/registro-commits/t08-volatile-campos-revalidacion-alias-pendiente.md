# Tanda 8: revalidación de campos volátiles y diagnóstico de alias

Con el compilador incremental de A: `3.0.0+build.20261108T124019Z.f076ba253139a69f`,
`pruebas-tanda-08-volatile-campos-c/validar.cmd` terminó con código 0: ejecutó
los caminos directo, con encabezado y con metadata en O0–O3 y sus comparaciones
de IR con Clang. Es evidencia de ese control, no de toda la tanda.

En la misma revisión, `diagnosticar-var-volatile.cmd` terminó con código 1.
En O0–O3, las lecturas y escrituras mediante alias de un arreglo flexible C
siguen sin el orden acquire/release que emite Clang en modo Microsoft; en modo
ISO las comparaciones de este control coinciden. Las referencias mixtas siguen
sin acreditar el modo Microsoft. La opción `CppConsumerVolatileMode` conserva
la barrera que rechaza su uso mientras no exista generación completa.

La prueba `nsc_project_method_body_model_tests` sigue fallando en una de 118
comprobaciones, sobre estadísticas de ondas paralelas, incluso en ejecución
aislada. No se atribuye ese fallo al cambio de alineación sin más evidencia.
Siguiente acción: completar el contrato de alineación y propagación del modo
para alias, sin quitar la barrera hasta verificar los caminos y regresiones.
