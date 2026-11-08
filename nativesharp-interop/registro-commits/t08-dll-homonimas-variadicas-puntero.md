# Punteros a funciones variádicas de DLL TLS homónimas

La asignación de una función C importada a un puntero fallaba en el análisis
porque se exigía una definición NativeSharp. El backend tampoco podía usar el
intermediario variádico para obtener una dirección válida. Se admite el símbolo
importado con identidad C y, para la ruta de DLL TLS homónimas, se resuelve la
exportación de la DLL correspondiente al crear el puntero. Las funciones nativas
siguen usando sus guardas de callback existentes.

La prueba deriva el control de llamadas variádicas: cada productor declara un
typedef de puntero distinto, guarda su propia función y la invoca con seis
argumentos. Los consumidores precompilados y restaurados desde caché ejecutaron
correctamente en O0–O3; cada función observó su propio estado TLS. Se añadió
el control al validador del bloque.

Al usar el mismo nombre para ambos typedefs, la muestra falla antes de analizar
los cuerpos con un conflicto de alias equivalentes. El caso de typedef homónimo,
otras variantes de puntero y otros tipos de consumidor permanecen pendientes.
