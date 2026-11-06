# Alcance de la corrección de arreglos volatile int

La comparación directa de IR identificó una diferencia de orden en lecturas
y escrituras de `int` de arreglo global C. Se limita la corrección a raíz de
global externo con modo MS y alineación demostrada, conservando el cálculo de
desplazamientos de cada dimensión. Las demás rutas y NSLink se mantienen.
La validación de la corrección será agrupada con la tanda 08.
