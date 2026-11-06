# Comprobación precisa del puntero volatile

El script exige una carga volatile del puntero global y una escritura ordinaria
del valor 17 en su objeto, además de impedir la escritura volatile de ese mismo
valor. También verifica el diagnóstico específico de la dirección del campo
volatile. Pendiente de repetir la validación agrupada.
