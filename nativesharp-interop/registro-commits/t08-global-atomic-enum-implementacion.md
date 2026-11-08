# Tanda 8: representación de globales C con enum atómico

El importador conserva la identidad del enum cuando Clang informa un almacenamiento
atómico cuyo tamaño coincide con el del enum, con tamaños de 8, 16, 32 o 64 bits
y alineación suficiente. HIR conserva la alineación verificada y el backend usa
el tamaño del tipo subyacente al emitir cargas y escrituras `seq_cst`.

El caso inicial de enum de 32 bits pasó de O0 a O3. La comparación del IR de
Clang y NSC comprobó carga y escritura `atomic i32 seq_cst` con alineación 4.
Evidencia: `A:\nsc-interop-build\tanda08-atomic-enum-25843-25725`.
Esto no demuestra todavía los demás anchos, TLS, DLL ni operaciones compuestas;
la tanda permanece abierta y esas variantes requieren validación adicional.
