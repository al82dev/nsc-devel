# T08: reproducción de operación compuesta en `_Bool` C escalar

Con el NSC actual, `flag |= true` sobre `extern volatile _Bool flag` falla
con `compound assignment requires compatible numeric operands`; registro
`A:/nsc-interop-build/t08-bool-scalar-repro/compound-nsc.log`. Clang 23 C11
compiló la función `compound_scalar` de `BoolCompoundReference.c`: su IR
usa carga acquire de byte, promoción a i32, operación, normalización y
escritura release de byte. IR en `compound-clang.ll` junto al registro.

La reproducción identifica el caso escalar; no demuestra las demás operaciones
ni sus resultados desde NSC. La implementación y validación siguen pendientes.
