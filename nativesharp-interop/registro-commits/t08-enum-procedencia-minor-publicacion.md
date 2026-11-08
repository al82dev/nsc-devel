# T08: conservar la versión de metadatos para procedencia de enums

Los controles de identidad y nombres fallaron al publicar catálogos de versión 7:
el productor generaba el nuevo catálogo pero rebajaba el minor de NSMETA a 45 o 47.
El lector interpretaba entonces el catálogo con el formato anterior y rechazaba la biblioteca.

La construcción conserva el minor 49 cuando el catálogo publicado es versión 7.
Los criterios de selección de las versiones anteriores permanecen iguales. La validación
agrupada de los controles afectados queda pendiente después de la compilación incremental.
