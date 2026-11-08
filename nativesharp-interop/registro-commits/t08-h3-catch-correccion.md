# T08 H3: ámbito de variable de excepción

El control `0da40cc5b` mostró en O0–O3 una global C publicada por error cuando
una variable de `catch` usa su nombre. Al clasificar demandas de una plantilla,
se reconoce ahora el nombre de la excepción únicamente en el cuerpo y el filtro
de su cláusula, siguiendo el ámbito usado por el analizador semántico.

La construcción incremental y la validación posterior están pendientes; este
registro no cierra el caso ni la tanda.
