# Prueba mixta de matriz C empaquetada

El fixture prepara un campo `int[2][3]` bajo `pack(1)`, compara tamaño y
offset con C, escribe y lee el elemento `[1][2]` y compara su valor desde C.
El IR debe usar alineación 1 en lectura y escrituras, incluida la compuesta;
un caso negativo intenta exponer la dirección del elemento. Los casos aún no
se han ejecutado ni acreditan arreglos de otros tipos o calificados.
