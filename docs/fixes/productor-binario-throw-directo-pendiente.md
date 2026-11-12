# Productor binario con throw explícito: regresión pendiente

Base de preparación: b64583272024a3fd50997ea828291c5675dcddcf.
La prueba y las fuentes están preparadas, no compiladas ni ejecutadas.

El productor declara campos Rc, WeakRc, Arc y WeakArc e interrumpe el
constructor explícitamente después de cada asignación; una variante completa
la construcción. El consumidor construye directamente el tipo importado para
aislar la limpieza del productor de las barreras de las fábricas compartidas.

El runner recibe por orden rutas explícitas a nsc, Core.lib, Core.nsmeta y un
directorio de evidencia nuevo; todos deben estar en A. Crea TEMP y TMP dentro
de esa carpeta y registra comandos, salidas, códigos, tiempos excedidos y hashes
de las entradas antes y después. Compila productor O0/O2 y consumidor O0/O2,
comprueba los cuatro puntos de fallo y el éxito, y espera retorno 42. No
modifica los fixtures originales.

La evidencia histórica de `constructor-campo-compartido-binario.md` usa
un fallo inducido por inicialización estática: 32 combinaciones de biblioteca
estática y otras 32 de DLL reportadas en `prestamo-get-punteros.md`. No es
evidencia de esta variante ni de la revisión actual. Las pruebas preparadas no
demuestran liberación física del bloque, compatibilidad del certificado de
cleanup entre productores y consumidores, ni comportamiento con destructores
que lanzan. La barrera de fábrica se mantiene.
