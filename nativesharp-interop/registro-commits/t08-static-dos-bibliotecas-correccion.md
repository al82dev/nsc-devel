# Visibilidad de globales C internas restauradas

La restauración de dependencias C asignaba visibilidad pública a todas las
globales, incluso cuando su declaración tenía enlace interno. Al analizar la
plantilla de cada productor, la global interna del otro productor aparecía como
candidata y causaba ambigüedad.

`provideCppValuesWithPrecompiledCatalogs` conserva la visibilidad pública de declaraciones
externas y usa visibilidad interna para las que Clang identificó con enlace
interno. No cambia el contenido ni el formato de la metadata.

La reproducción de dos bibliotecas pasó en O0 y el consumidor observó 11 y 29.
También pasaron los controles existentes de global interna en plantilla, global
externa en plantilla y ambigüedad de global interna. La construcción incremental
informó tres advertencias por inicializadores en `CppSemanticRecord.cpp`, fuera
de las líneas modificadas.
Esta verificación no acredita otras optimizaciones ni cierra T08-002.
