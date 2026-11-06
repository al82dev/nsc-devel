# Prueba de union C con campos de bits volatile

Se amplía el fixture con una union cuyos campos volatile se superponen a un entero. La validación comprueba lecturas, escrituras y valor resultante desde C; el control IR compara una lectura y una escritura equivalentes con Clang. El resultado debe obtenerse al ejecutar el grupo completo.

`validar-volatile.cmd` pasó O0–O3 y el control IR confirmó que Clang y NSC usan una carga volatile i32 para leer y una carga seguida de escritura volatile i32 para escribir, alineadas a cuatro, para las funciones de este fixture. No implica cobertura de todos los layouts de union.
