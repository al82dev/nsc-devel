# T08-006: campos C de enum volatile

## Problema y cambio

La proyección de un campo C `volatile` con tipo enum nombrado llegaba a
`projectCppSemanticEnumType`, que rechaza expresamente tipos volatile. La
proyección de arreglos C ya limpia ese calificador antes de resolver el enum y
lo restituye después. Se aplica el mismo tratamiento al campo escalar en
`projectCppSemanticInstanceField`: el mapeo conserva la identidad comprobada,
el spelling original y `volatile`. La alineación efectiva sigue calculándose
por la ruta existente. El caso enum anónimo conserva su camino anterior.

El lowering de campos C ya reconoce enums mediante `findEnumUnderlyingInfo`
para determinar el ancho del acceso volatile; este cambio no altera el lowering.
No se modifica el formato NSMETA, NSLink, LLVM/Clang ni la instalación de C:.

## Pruebas preparadas y estado

El fixture `pruebas-tanda-08-volatile-campos-c` incorpora un enum con signo en
un struct, un struct `pack(1)` y una union. Prepara lecturas y escrituras
cruzadas C/NativeSharp, consumidores precompilados y comparación de orden,
volatile, tipo y alineación del IR contra Clang. El validador existente cubre
O0–O3 y las rutas directa, compartida y solo metadata.

**No se han construido ni ejecutado estos cambios.** El resultado, la sintaxis
del fixture y la compatibilidad precompilada permanecen sin verificar. La
validación se agrupa con el trabajo pendiente de la tanda 8. Otros calificadores,
tipos de enum y combinaciones no quedan acreditados por este cambio.
