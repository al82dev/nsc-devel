# Emisión del almacenamiento de puntero global volatile

T08-015: la ruta de carga y escritura directa usa acquire/release para un
puntero de ocho bytes cuando MIR conserva la prueba de alineación y el modo MS
de Clang. El acceso al objeto apuntado mantiene su propia calificación. Esto no
extiende el tratamiento a punteros indirectos ni campos. Falta la validación
agrupada con los casos de C y NativeSharp.
