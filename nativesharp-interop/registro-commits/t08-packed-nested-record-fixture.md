# Prueba preparada de record C anidado y empaquetado

El fixture prepara un `Pair` naturalmente alineado dentro de un record
`pack(1)`. Compara tamaño y offset con C, escribe y lee el campo interior y
comprueba el resultado desde C. Un caso negativo intenta tomar su dirección.
El IR de alineación 1 se comprueba junto con los otros casos del fixture.
La prueba todavía no se ha ejecutado; otras formas anidadas siguen pendientes.
