# Retorno de enum C mediante typedef de función

La muestra de dos DLL homónimas se amplió para que el puntero a función variádica
reciba y devuelva `CKind`. Cada resultado incorpora el TLS y los argumentos de
su DLL; NativeSharp convierte el resultado a entero y lo compara con el control C.

Pasaron O0–O3 con encabezados ocultos y restauración desde caché. En O0 se
rechazó la variante donde un segundo enumerador difiere entre los productores.
Se incorporaron ambos controles al validador del bloque; no se modificó el
compilador ni se ejecutó la batería completa de la tanda. El resultado no cubre
enums anónimos ni todas las representaciones y calificadores posibles.
