# Campos de enum C++ dentro de namespaces

Un campo cuyo tipo era `T09Cpp::Mode` no aparecía en el record importado,
aunque el tipo y su enumerador ya funcionaban. Reproducción aislada:
`A:/temp/t10-campo-enum-namespace-sonda/resultado.log`.

La identidad del campo conserva los segmentos de namespace de Clang y se
concilia con el enum importado, su USR y su disposición. La selección de
dependencias del record incorpora el enum correspondiente. La ruta existente
de selección para campos C conserva su tratamiento de calificadores.

`pruebas-tanda-10-enum-cpp-namespace/validar.py` compara Clang 23.1.0 y
NativeSharp en O0–O3, leyendo y escribiendo campos de dos records triviales
cuyos enums homónimos pertenecen a namespaces distintos. También prueba el
uso del campo sin declarar explícitamente el tipo enum. Resultado:
`A:/temp/nsc-t10-enum-cpp-namespace-sme68s6n`. La regresión de enum global y C
pasó en `A:/temp/nsc-t10-enum-cpp-global-_5htm1h0`.

La evidencia cubre enums nombrados no scoped de base `int`; no acredita el
paso por copia ni precompilación de estos campos. T11-004 y T09-003 siguen
abiertos.
