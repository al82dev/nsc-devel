# Orden MS para arreglos globales C escalares

La marca de alineación y el reconocimiento de elementos se amplían a los
tipos enteros y flotantes con anchos comprobados por Clang. Se exige origen C,
modo MS, tipo de raíz compatible y que todos los strides preserven la
alineación. `_Bool`, campos y punteros siguen fuera de esta ruta. Los
controles y la validación agrupada están pendientes.
