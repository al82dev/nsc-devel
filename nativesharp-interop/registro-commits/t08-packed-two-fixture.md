# Prueba preparada de campos C con pack(2)

El fixture compara con Clang el layout de un record `pack(2)` con campos
`short`, `int` y `double`, modifica los valores y comprueba el resultado en C.
Exige alineación 2 en el IR para int y double y toma la dirección del short,
cuya alineación natural sí está garantizada. La prueba aún no se ha ejecutado;
no acredita otras formas de campos empaquetados.
