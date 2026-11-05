# Tanda 08: prueba preparada de campos C superpuestos

Una union C con miembros `int` y `float` permite comprobar que Clang informa
offset cero para ambos y que la proyección conserva ese offset. También se
comprueba que los campos C se tratan como accesibles. La prueba no demuestra
todavía lectura o escritura ejecutada y se correrá al validar toda la tanda.
