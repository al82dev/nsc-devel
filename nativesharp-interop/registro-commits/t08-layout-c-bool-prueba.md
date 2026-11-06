# Prueba mixta de `_Bool` C

El fixture compara tamaño, alineación y offsets de un struct con campo y
arreglo `_Bool`. C inicializa los datos y NativeSharp los lee y modifica;
después C comprueba los valores. El control está preparado para O0–O3, pero
no se ha ejecutado y puede revelar trabajo adicional en la representación.
