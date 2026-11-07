# T08-015: admisión de operando `int` en `_Bool` C compuesto

La referencia de Clang registrada en `t08-bool-compuesto-referencia.md`
promueve `_Bool` a `int` antes de operar y lo reconvierte según sea cero o no.
El analizador admite ahora seis operaciones compuestas con operando `int` en
el arreglo C global `volatile _Bool` y modo MS ya delimitado. El bool nativo
y otros orígenes conservan sus reglas anteriores.

El cambio está preparado, sin construir ni validar. El HIR debe conservar el
operando `int` y LLVM debe operar en i32 antes de convertir el resultado;
faltan esas partes y los controles agrupados. No se considera soporte terminado.
