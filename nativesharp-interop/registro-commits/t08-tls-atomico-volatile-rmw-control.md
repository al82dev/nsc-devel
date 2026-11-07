# Control de IR para actualización atómica volatile

Clang 23 emitió las lecturas y escrituras simples como `atomic volatile`, pero
el `+=` como `atomicrmw add` sin el marcador LLVM `volatile`. La primera prueba
exigía ese marcador también para la actualización y falló en el control C.
Se corrige la expectativa, manteniendo el orden `seq_cst`, la alineación y la
asociación al global; no se ha acreditado aún el resultado de NativeSharp.
