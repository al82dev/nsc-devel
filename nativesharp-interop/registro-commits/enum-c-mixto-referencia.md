# Operaciones mixtas de enum C unsigned e int signed

Se prepara una comparación y una suma frente a funciones C compiladas por
Clang con el mismo tipo enum. La prueba busca diferencias observables antes
de decidir cómo deben interactuar las promociones de C y NativeSharp.

El primer ejecutable devolvió 7 en O0: en NS el enum unsigned e int signed se
promueven a `long`; en C la comparación usa unsigned. HIR muestra conversiones
a `builtin:long` de ambos operandos. Es una diferencia de reglas numéricas;
se preguntó al usuario qué semántica desea. El caso está separado y no se
considera aprobado ni resuelto por el código de salida del lanzador.
