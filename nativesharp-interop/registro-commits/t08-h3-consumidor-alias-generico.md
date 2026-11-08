# Tanda 8: consumidor de alias C en plantilla

La prueba genera el catálogo preparado desde un proyecto con una referencia C
real. Después crea un módulo precompilado en memoria con el texto de la
plantilla y ese catálogo; un proyecto consumidor sin el encabezado solicita una
especialización y ejecuta `compileProjectMembers` con el proveedor de alias.

La compilación incremental y `nsc_cpp_c_identity_tests` terminaron con código
0. El archivo de metadata del fixture es un marcador y no un NSMETA serializado:
esta prueba no verifica el driver, la emisión normal, el enlace ni la ejecución.
Tampoco acredita colisiones de nombres, dependencias C distintas del alias ni
el cierre de la tanda.
