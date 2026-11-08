# Invocación con record C anidado

La prueba de dos DLL con typedef variádico se amplió para que `Payload` contenga
un `Inner` y la función C lea `payload->inner.value`. Cada DLL entrega un valor
distinto, que se combina con su TLS y los argumentos variables. Los controles C
y NativeSharp pasaron en O0–O3, con encabezados ocultos y restauración desde caché.

Una segunda muestra cambia `Inner.value` de `int` a `float` en un productor:
aunque ambos tienen el mismo tamaño, el consumidor rechaza la incompatibilidad.
La variante quedó incorporada al validador dirigido. No se modificó el compilador
ni se ejecutó la batería completa de la tanda. Retornos de record y enums siguen
pendientes.
