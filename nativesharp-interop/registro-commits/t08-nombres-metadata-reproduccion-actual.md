# Reproducción actual de nombres C por metadata

En `f9a5fa659`, con el compilador incremental `7dc4a8491734454f`, se
ejecutó `pruebas-tanda-08-nombres-metadata/validar.cmd`. Las cuatro sondas
de `NRecord` compilaron y ejecutaron en O0, y el consumidor `Shared` pasó
O0. El consumidor `Metadata` fue rechazado en O0 con identificadores
desconocidos `NAlias` y `NMood`; la ejecución terminó con código 1.

La inspección de cadenas del `CNamesProducer.nsmeta` generado encontró
`NRecord`, pero no `NAlias` ni `NMood`. El productor de este fixture publica
firmas que usan `NRecord`, sin usar los otros dos nombres. Por lectura de
`captureCppCTypeCatalog`, los records y enums se seleccionan desde tipos de
la API y otras dependencias registradas; el proveedor del parser reconstruye
los tipos presentes en el catálogo. Por tanto, el diagnóstico anterior de
que ningún tipo del catálogo llega al parser ya no describe este resultado.

La ausencia de esos nombres en el archivo no demuestra por sí sola qué
política de publicación debe aplicarse ni la causa completa del alias. Es
necesario investigar la publicación y restauración de ambos contratos antes
de modificar la selección. La prueba no llegó a O1–O3 y la tanda 8 continúa
abierta.
