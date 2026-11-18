# Layout de tuplas en campos estáticos

Un campo estático de clase de tipo tupla fallaba al generar LLVM si ninguna
función mencionaba ese tipo. El diagnóstico era `Stage 40 cannot map named
value type to LLVM carrier`; las reproducciones con elementos escalares, un
struct ordinario y `WeakArc` están en los registros `static-tuple-*-before.log`
de `A:/nsc-fixes-evidencia`. En la revisión ensayada, añadir un local de la
misma tupla permitía compilar y ejecutar (`static-tuple-use-control.log`). No
se comparó el defecto con un compilador construido de la revisión base.

`importTupleLayouts` recogía tuplas de las firmas y nodos de las funciones HIR,
pero el modelo MIR añade los campos estáticos después de importar layouts.
Por eso, cuando el tipo aparece únicamente en un campo, no se publica su
layout sintético para LLVM. La corrección recoge también los tipos de tupla de
campos estáticos cuyo layout puede calcularse y reutiliza la importación y la
deduplicación existentes. Los tipos sin layout calculable quedan diferidos;
no se cambia el cálculo de layout, el ABI ni el tratamiento de interoperabilidad.

`tests/integration/tuple_value_drop.py` comprueba campos estáticos escalares,
anidados, con struct y dentro de un tipo genérico abierto. Las ocho ejecuciones
O0/O2 terminaron con 42, al igual que los seis controles de destrucción de
tuplas (`static-tuple-independent.log`). La selección pública adicional pasó
para un campo de tupla con `WeakArc`, los controles de escritura y lectura y
los rechazos de `Rc` y `WeakRc` en almacenamiento compartido
(`static-tuple-layout-directed.log`). La prueba pública usa fuentes de Core.

El análisis dirigido `clang-tidy --checks=clang-analyzer-*` del importador
terminó con código 0 y nueve advertencias de padding en tipos de encabezados
incluidos (`static-tuple-analyze.log`); no se modificaron esos tipos en esta
corrección. Esta ejecución analiza el árbol con los demás cambios en curso y
no sustituye una comparación estática contra la revisión base.

Las pruebas usan el compilador incremental del árbol de trabajo, que contiene
otros cambios aún sin registrar; no se ha construido este commit por separado.
No se verificó la emisión de estado de namespace de tipo tupla: la sintaxis
directa se rechaza en parser y el alias ensayado se rechaza en resolución de
tipos. Tampoco se probó consumo precompilado ni la destrucción de un `WeakArc`
no vacío en la tupla al terminar el módulo. No se ejecutó batería completa.
