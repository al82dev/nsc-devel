# Suma mixta enum C unsigned e int

Se separa la suma del primer control de comparación para observarla de manera
independiente frente a Clang. La semántica deseada sigue en consulta; este caso
no determina por sí solo qué reglas debe adoptar NativeSharp.

Con `5bcb2fb4f02458e7`, el proyecto compiló y el ejecutable devolvió 8 en O0:
el resultado de la suma no coincide con el calculado por la función C. La
selección HIR observada en la prueba conjunta es `long` para ambos operandos;
la decisión entre reglas del lenguaje anfitrión y de C sigue pendiente.
