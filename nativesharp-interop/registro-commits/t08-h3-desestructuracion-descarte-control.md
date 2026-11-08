# T08 H3: control de patrón anidado y descarte

La prueba usa un patrón anidado con un descarte `_` y variables que ocultan una
global y una función C. El encabezado también declara una global llamada `_`.
Se verifica el resultado del consumidor y que el catálogo no capture esas
dependencias por los identificadores del patrón.

Con el compilador anterior a la corrección, el diagnóstico terminó con código 1:
en O0–O3 la global `_` se publicó indebidamente. La ejecución del consumidor y
las demás comprobaciones finalizaron antes de ese resultado.
