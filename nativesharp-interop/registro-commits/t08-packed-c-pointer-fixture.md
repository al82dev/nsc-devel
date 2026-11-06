# Prueba mixta de campo puntero C empaquetado

El fixture prepara un `int*` guardado en un record `pack(1)`. NativeSharp lee
el puntero, modifica su objeto apuntado, cambia el puntero y vuelve a comprobar
el valor mediante C. El IR debe cargar y escribir el campo puntero con
alineación 1. Un caso negativo intenta exponer su dirección como `int**`.
La prueba todavía no se ha ejecutado y no cubre todos los tipos apuntados.
