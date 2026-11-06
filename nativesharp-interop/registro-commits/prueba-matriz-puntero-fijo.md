# Matriz de punteros constantes y doble const

El fixture agrega `int * const[2][2]` y `const int * const[2]`. Se comprueba
lectura de ambos, escritura de datos mutables y rechazo de modificaciones a
slots o datos constantes, así como de pasar una fila a `int**`. Pendiente de
ejecutar y diagnosticar; no se afirma soporte.

Con `f3f7271196c4666b` pasaron lectura y escritura válida en O0–O3 y la
verificación desde C. Se rechazaron reasignaciones, escritura al dato const,
escape a puntero mutable y argumento de fila mutable. Las direcciones de un
slot de matriz y de doble const también fueron rechazadas por los diagnósticos
esperados. La evidencia cubre estas formas, no todos los arreglos C.
