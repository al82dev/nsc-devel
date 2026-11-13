# Enumeradores de enum C++ global: avance parcial

La expresión `Mode value = Chosen` fallaba con identificador desconocido aun
con el tipo global importado; reproducción previa en
`A:/temp/t10-enumerador-cpp-sonda/resultado.log`. El inventario excluía los
enumeradores C++ y la generación de constantes estáticas no aceptaba su tipo.

La proyección admite únicamente enumeradores del enum global no scoped con
base `int` ya importado, con identidad y disposición verificadas por Clang.
El valor conserva el tipo enum del proyecto y se inicializa como entero de su
base. No se aplican las conversiones propias de enum C.

`pruebas-tanda-10-enum-cpp-global/validar.py` compara con Clang 23.1.0 los
valores 10, 11 y -7, ejecutando los programas C++ y NativeSharp en O0–O3.
También pasa la regresión equivalente de C en O0 y se conservan rechazos de
enum scoped, namespace y base sin signo. Otro control verifica el rechazo de un
enumerador de un tipo enum distinto. Resultado `OK`, registros en
`A:/temp/nsc-t10-enum-cpp-global-bgwwxy25`; compilación incremental código 0.

No se acreditan enumeradores scoped o de namespace, otras bases, precompilación,
campos de record ni su ABI. T09-003, T10-002 y T11-004 siguen abiertos.
