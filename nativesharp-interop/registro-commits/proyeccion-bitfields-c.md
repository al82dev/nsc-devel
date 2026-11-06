# Proyección inicial de bitfields C no volátiles

Se preparó la proyección de campos enteros de structs C como ventanas de bytes
acotadas por el tamaño del objeto, derivadas de las coordenadas AST de Clang.
Se rechazan ventanas que se solapan con campos ordinarios, tipos o anchos no
representables, unions y volatile. El transporte y descenso aún no están
completos ni se ha validado el comportamiento.
