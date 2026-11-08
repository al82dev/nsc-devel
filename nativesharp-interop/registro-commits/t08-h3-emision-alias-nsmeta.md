# Tanda 8: emitir dependencias de alias C

El productor solicita el catálogo de dependencias preparado y emite NSMETA
1.46 cuando contiene candidatos de alias de plantillas públicas. Si el catálogo
está vacío o usa un formato anterior, conserva la versión mínima que ya
correspondía al contenido. El lector reconoce 1.46.

La construcción incremental de NSC y la prueba de biblioteca real del commit
`596f43e30` pasaron en O0: se verificó el alias dentro del NSMETA, se compiló
un consumidor sin encabezado C y su ejecutable retornó cero. Las pruebas de
colisiones, O1–O3, DLL, funciones y globales y la validación completa de la
tanda permanecen pendientes. El catálogo no demuestra por sí mismo que una
referencia candidata fuera seleccionada semánticamente.
