# T08: controles históricos con procedencia de enums

Los productores con procedencia de enums ahora publican NSMETA 1.49 y catálogo 7.
El control suponía que todos los productores usaban catálogos 1 a 4, por lo que
rechazaba esos productores antes de ejecutar sus controles de versiones históricas.

El control comprueba el minor 49 del catálogo 7 y reconstruye el catálogo anterior
desde los tipos decodificados para conservar las comprobaciones 1.42 a 1.45. No
afirma que los formatos anteriores preserven la nueva procedencia. Validación pendiente.
