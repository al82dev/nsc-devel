# Desplazamiento y operadores de enum C

Se preparan controles de desplazamiento unsigned, máscara y negación de enum C.
Se mantienen las comparaciones con Clang y el rechazo de enum NativeSharp.
Pendiente ejecutar y delimitar qué operadores requieren cambios.
Se agrega enum C de base unsigned de 8 bits para verificar promoción a int en
las tres rutas y `_Static_assert` de su tamaño desde Clang.
Se añaden complemento, máscara entre dos enums C y desplazamiento cuyo conteo
es enum C para cubrir ambos operandos y la promoción unaria.
