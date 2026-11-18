# Productor binario con throw explícito: regresión pendiente

## Validación posterior

Con la revisión `1bd04bae9`, el runner preparado compiló el productor en O0 y
O2 y el consumidor en O0 y O2 para los cuatro puntos de fallo y el éxito:
20 ejecuciones, todas con salida 42. Las fuentes y comandos de cada caso,
salidas, hashes previos y posteriores y lista de fallos vacía están en
`A:/nsc-fixes-evidencia/shared-binary-throwing-direct-1bd04bae9/`; el
lanzador y resumen tienen el mismo nombre con extensiones `.cmd` y `.log`.
Se usaron compilador aislado SHA-256
`9dafe59b7ca23344db5ae5fa4cbbc107068fdf4d6e6ac9c482ca02857a04df87`, Core
de `integracion-vistas-sdk` SHA-256
`565bf2177c85f88d94057ebe8616f2caeb71e01b7e39610bf815c15b685221c6` y
metadata SHA-256
`7d5921ca90d909c1e7fa98d092b9c0c28104dfe56f60f940e5deb89096203ccf`.
Las rutas de runtime e imports están fijadas en el lanzador. Esto valida los
casos de esta matriz con el productor actual; no demuestra liberación física,
compatibilidad con otros productores ni destructores que lanzan. No se
ejecutó batería completa ni interoperabilidad C/C++.

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
