# Tanda 08: sizeof de record C incompleto

El análisis semántico detecta el tipo importado declarado sin definición antes
de crear la operación de `sizeof` o `alignof`. El lookup usa la identidad exacta
del valor, de modo que un puntero al tipo no coincide con esa identidad. El
fixture de proyecto está preparado y aún no se ha ejecutado.
