# Alcance del orden MS para arreglos escalares C

La referencia Clang muestra acquire/release para los enteros y flotantes
declarados en el fixture, incluidos unsigned int de 32 bits. Se extiende la
ruta del global C alineado a esos anchos. La forma de almacenamiento de
`_Bool` se revisa aparte; no se declara cubierta por esta modificación.
