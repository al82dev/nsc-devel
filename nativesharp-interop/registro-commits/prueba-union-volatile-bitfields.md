# Prueba de union C con campos de bits volatile

Se amplía el fixture con una union cuyos campos volatile se superponen a un entero. La validación comprueba lecturas, escrituras y valor resultante desde C; el control IR compara una lectura y una escritura equivalentes con Clang. El resultado debe obtenerse al ejecutar el grupo completo.
