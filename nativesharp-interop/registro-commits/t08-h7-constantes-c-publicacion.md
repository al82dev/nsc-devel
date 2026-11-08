# Tanda 8: publicación de bibliotecas que usan constantes C

Se excluyen los enumeradores C importados de los estados de namespace
publicados: no son almacenamiento NativeSharp. Esto corrige el rechazo del
productor por un estado sin namespace, sin modificar el formato de metadata.

La construcción incremental de `nsc` y `nsc_cpp_c_identity_tests` pasó. El
validador del carril terminó con código 0: los métodos ordinarios con constantes
C con nombre o anónimas se publicaron y ejecutaron, y la inspección confirmó
que los productores probados no publicaron esos estados. Evidencia en
`A:\nsc-interop-build\tanda08-identidad-h7-20261107`.

Las plantillas que necesitan constantes ausentes del catálogo siguen rechazadas
en el consumidor; H3/H7 permanecen abiertos. El validador contiene rechazos
esperados y esta ejecución no acredita toda la tanda 8.
