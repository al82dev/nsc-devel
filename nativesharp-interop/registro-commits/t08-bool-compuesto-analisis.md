# T08-015: admisión semántica de `_Bool` C compuesto

El fixture `VolatileBoolArrayCompound.ns.txt` reproduce el rechazo de `|=` en
un arreglo C `volatile _Bool`; Clang acepta y ejecuta esa forma. El analizador
admite `&=`, `|=` y `^=` entre bool solo cuando el destino es un índice directo
de un arreglo global C volatile con modo MS. La regla de bool NativeSharp y
los demás orígenes permanecen sin ampliar.

Este cambio por sí solo no completa la operación: faltan el descenso HIR, la
lectura y escritura i8 ordenadas en LLVM, y los controles agrupados. No se ha
construido ni validado esta etapa; no se considera interoperabilidad terminada.
La referencia C y sus límites están en `t08-bool-compuesto-referencia.md`.
