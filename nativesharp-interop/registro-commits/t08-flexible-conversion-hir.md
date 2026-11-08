# T08: dirección del miembro flexible en HIR

Una dirección explícita de elemento flexible usa `address-value` sobre una
dirección de almacenamiento. Para la conversión contextual preparada, HIR
transforma el campo importado de valor a dirección y lo envuelve como puntero,
sin cargar ni copiar el arreglo.

La selección semántica todavía no está conectada; falta comprobar ejecución,
calificadores, alineación y variantes de expresiones antes de acreditar soporte.
