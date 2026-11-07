# T08-002: global C `static` de encabezado y T08-004

Se añade `T08-002-GLOBAL-STATIC.md`. Recoge la identidad Clang del global
`static` de encabezado (USR con el nombre base del archivo, único para
homónimos de valor distinto), su no importación (`unknown identifier`, sin
diagnóstico propio) y la causa en la selección de globales.

Para T08-004 aporta la semántica C (una copia por unidad de traducción), el
requisito de identidad por proyecto y referencia, y la decisión de diagnóstico
pendiente. Solo documentación: no se toca el carril de variables globales.
