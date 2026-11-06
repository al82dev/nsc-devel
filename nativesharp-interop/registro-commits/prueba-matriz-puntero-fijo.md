# Matriz de punteros constantes y doble const

El fixture agrega `int * const[2][2]` y `const int * const[2]`. Se comprueba
lectura de ambos, escritura de datos mutables y rechazo de modificaciones a
slots o datos constantes, así como de pasar una fila a `int**`. Pendiente de
ejecutar y diagnosticar; no se afirma soporte.
