# Control ThinLTO y objetos de ciclo de vida

El control anterior esperaba que toda emisión de objetos C++ se rechazara al
usar ThinLTO. Esa expectativa dejó de corresponder al compilador cuando se
permitieron objetos nativos en paquetes sin internalización.

El control comprueba ahora que la emisión agrega los objetos esperados en O0
y O2 cuando no hay internalización. Además comprueba que O2 con internalización
rechaza la emisión sin agregar objetos. No se modificó código de producción ni
las pruebas de NSMETA.

La construcción incremental del ejecutable de pruebas terminó correctamente.
La ejecución ya no reportó los dos fallos ThinLTO anteriores ni fallos en el
control de internalización. El ejecutable todavía terminó con código 1 por tres
fallos de dependencias de enums; la tanda 8 sigue abierta.
