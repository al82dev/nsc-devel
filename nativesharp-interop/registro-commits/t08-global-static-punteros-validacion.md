# T08-004: validacion de punteros C internos

La construccion incremental y la referencia C pasaron. NativeSharp leyo y
modifico objetos mediante punteros internos a record y enum, y llamo un
callback inicializado con una funcion C interna en O0–O3. Las funciones inline
observaron el mismo estado. Pasaron las regresiones de callbacks, globales
internas complejas y calificadores.

El control se agrega al gate de tanda, todavia no ejecutado por completo. No
se acreditan otras firmas de callback, punteros calificados ni formas TLS.
