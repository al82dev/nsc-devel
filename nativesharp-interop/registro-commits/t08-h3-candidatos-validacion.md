# T08 H3: validacion de candidatos en plantillas

Se compilo incrementalmente `nsc_cpp_c_identity_tests` tras el cambio de estructura del modelo generico. La compilacion reconstruyo sus dependencias afectadas y termino con codigo 0. La ejecucion con sesiones Clang 23.1 y temporales en A: tambien termino con codigo 0.

El control verifica que la captura incluye referencias sintacticas a funcion, global y alias en el namespace esperado, y tambien una referencia sombreada por un local. Esto acredita que se preservan candidatos, no que esten resueltos semanticamente ni que se publiquen o restauren. La seleccion real y el gate de tanda 8 siguen pendientes.
