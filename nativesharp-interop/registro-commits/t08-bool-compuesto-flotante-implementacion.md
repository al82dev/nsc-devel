# T08-015: `_Bool` C volatile compuesto con flotantes

El análisis, HIR y LLVM preparan `+=`, `-=`, `*=` y `/=` con `float` y
`double` para el arreglo C global `volatile _Bool` en modo MS. El descenso
convierte bool al tipo flotante, opera y usa comparación `une` con cero para
normalizar, conservando NaN como verdadero y los accesos byte acquire/release.
El bool NativeSharp y operadores no aritméticos no se amplían.

La implementación aún no se ha construido ni validado; faltan controles
O0–O3 y de IR, incluido NaN. No se modificó NSLink.
