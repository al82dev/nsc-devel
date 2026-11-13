# SysV: campo enum C++ en namespace precompilado

El productor reducía `T09Cpp::Mode` a `Mode` al construir la clausura del
record, y no encontraba ese enum en el catálogo. Tras corregir la captura, la
plantilla consumidora también necesitó restaurar el enumerador en su namespace
y conciliar la identidad calificada del campo con el plan SysV.

La captura, el catálogo vigente y la restauración conservan nombre completo,
origen C++, USR y disposición. El consumidor comprueba el enum del mismo
catálogo y el tipo instalado antes de admitir el plan físico. Se conserva la
representación y selección existentes para C; no se agrega selección de
contratos por etiquetas históricas.

`pruebas-tanda-09-sysv-cpp-copia-enum-namespace/validar-precompilado.py` pasó
las 16 combinaciones: estática y DLL O0–O3, con y sin encabezado. Registros:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-_4kvbi78`. El control
de identidad del catálogo aprobó la recuperación del namespace y rechazó uno
distinto; también pasaron transporte y ABI. Las 16 combinaciones del enum
global y la regresión C pasaron en
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-6wkvessc` y
`A:/temp/nsc-t10-enum-cpp-global-zhvypj6i`.

Se acredita un enum nombrado no scoped de base `int`, en un record y una firma
concretos. No se acreditan otras bases, enums scoped ni interoperabilidad
general. T09-003, T10-002 y T11-004 siguen abiertos.
