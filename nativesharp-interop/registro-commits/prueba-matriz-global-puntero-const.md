# Matriz C global de punteros a datos const

Se prepara una matriz `const int *[2][2]`: la prueba mixta lee un elemento,
reasigna el slot y verifica el nuevo valor desde C. Dos controles intentan
escribir al dato const y convertir un elemento a `int*` mutable. Falta ejecutar
la prueba y examinar los diagnósticos antes de modificar el compilador.
