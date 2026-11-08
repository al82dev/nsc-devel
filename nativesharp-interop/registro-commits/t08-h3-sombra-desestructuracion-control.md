# T08 H3: control de sombra por desestructuración

Se amplía el productor de plantillas con una declaración `var (shadow_value,
other)` y se comprueba su resultado desde la biblioteca precompilada. El
validador existente verifica además que la global C `shadow_value` no se
publique cuando todas sus referencias son locales.

Este control aún no acredita el resultado: se ejecutará junto con la corrección
de la tanda, en los cuatro niveles de optimización.
