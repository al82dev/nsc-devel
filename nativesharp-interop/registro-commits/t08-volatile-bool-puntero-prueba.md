# Tanda 8: puntero C a bool volatile

Se amplía el control de `_Bool*` con lecturas y escrituras indexadas y por desreferencia de una global `volatile _Bool*`. El comparador exige `load atomic volatile i8 ... acquire` y `store atomic volatile i8 ... release` observados en Clang para el objetivo Windows x64.

La línea base falla en NSC por ausencia de acquire; antes de agregar el requisito de IR, la ejecución O0–O3 pasó. Evidencia: `A:\nsc-interop-build\tanda08-bool-pointer-18668-4436`. La corrección se registra y valida aparte.
