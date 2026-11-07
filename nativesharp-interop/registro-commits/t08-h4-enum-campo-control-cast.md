# T08-002 H4: ajuste de control de asignación de enum nombrado

El primer intento se detuvo en el parser al no conocer todavía el nombre del
tipo en una conversión. El control obtiene el valor tipado de una función C
inline, sin eliminar la declaración local que comprueba el tipo importado.
El fallo semántico de H4 requiere nueva reproducción.
