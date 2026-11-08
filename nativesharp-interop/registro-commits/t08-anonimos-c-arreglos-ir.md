# Inspección de alineación de arreglos anónimos C

Tras la ejecución registrada en `t08-anonimos-c-arreglos-validacion.md`, se
inspeccionaron los doce archivos `Project`, `Repeat`, `Shared` y `Metadata`
de `A:/nsc-interop-build/tanda08-anonimos-c/*-volatile-O{1,2,3}.ll`.
En todos, la escritura de `items[1]` y su lectura usan `i32` con alineación 4;
la escritura y lectura de `packedItems[1]` usan `i16` con alineación 1.

En `clang-volatile.ll`, generado por Clang 23.1.0 para `Api.c` con O1, la
lectura y escritura del elemento natural usan alineación 4 y las del
empaquetado alineación 1. La comparación se hizo mediante inspección de IR,
no mediante una aserción automatizada. O0 y otros índices no se compararon en
IR. Esta evidencia no cierra los ítems ni la validación de la tanda 8.
