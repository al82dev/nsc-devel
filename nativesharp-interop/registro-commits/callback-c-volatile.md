# Callback C volatile almacenado

Se prepara un fixture de lectura y escritura de punteros a función volatile en
un global y un campo C, con ejecución cruzada, y emisión de IR de Clang y NSC
para comparar semántica. Está pendiente de ejecución; producir IR no basta para
acreditar el orden de memoria: debe inspeccionarse y añadirse una comprobación.
