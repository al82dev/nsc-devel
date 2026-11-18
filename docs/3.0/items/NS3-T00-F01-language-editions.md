> **Aviso vigente sobre NSMeta:** NSMeta no se versiona ni se acepta o rechaza por una versión;
> su compatibilidad se determina por el contenido y los contratos aplicables.

# NS3-T00-F01 — Lenguaje acumulativo

NativeSharp tiene un único lenguaje acumulativo. Los proyectos no seleccionan una versión del
lenguaje y el compilador no debe activar, ocultar ni rechazar sintaxis en función de una etiqueta.
La versión del producto es 1.0 y es independiente de la identidad de cada compilación.

`ProjectLoader` admite la propiedad antigua `Language` como entrada de compatibilidad y no le
atribuye semántica. Los proyectos sin esa propiedad y los que contienen valores distintos deben
tener acceso a la misma superficie del lenguaje. La metadata precompilada se valida por su
contenido y sus contratos de destino y ABI, sin una edición de lenguaje.

Las pruebas de `tests/unit/NativeSharpT00ConstitutionTests.cpp` y de inferencia de proyectos
comprueban la neutralidad de esa propiedad y el comportamiento del grafo. Una ejecución histórica
del objetivo de constitución registró 31 comprobaciones aprobadas para una implementación anterior;
ese resultado no valida el estado actual. La compilación y las pruebas actuales están pendientes de
ejecución por el usuario.

Archivos principales: `src/Compilation/ProjectLoader.cpp`,
`src/Compilation/ProjectGraphLoader.cpp` y `src/Interop/LibraryMetadataCodec.cpp`.
