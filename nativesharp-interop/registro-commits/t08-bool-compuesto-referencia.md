# T08-015: referencia de asignación compuesta a `_Bool` C

El control negativo existente `VolatileBoolArrayCompound.ns.txt` usa una
operación compuesta sobre un elemento de un arreglo C `volatile _Bool`. NSC
lo rechaza con `compound assignment requires compatible numeric operands`;
el gate también protege el rechazo de una operación compuesta sobre bool
nativo. Este registro no cambia ninguna de esas pruebas ni afirma soporte.

Clang 23.1.0 compiló `pruebas-tanda-08-record-c/BoolCompoundReference.c`
para `x86_64-pc-windows-msvc`, C11 y O0. Para `+=`, `-=`, `^=` y `*=`,
el IR observado carga `i8` con `atomic volatile acquire`, convierte a bool y
lo promueve a `i32`, aplica la operación, compara el resultado con cero,
almacena `i8` con `atomic volatile release` y devuelve el valor convertido.
No es un `atomicrmw`: el objeto es `volatile _Bool`, no `_Atomic(_Bool)`.

Comando: `A:/al82dev-clang/bin/clang.exe --target=x86_64-pc-windows-msvc
-std=c11 -O0 -S -emit-llvm nativesharp-interop/pruebas-tanda-08-record-c/BoolCompoundReference.c
-o A:/nsc-interop-build/t08-bool-compuesto-referencia.ll`.
El IR observado en esa ruta se generó con el mismo contenido antes de agregar
el archivo al repositorio. La corrección NSC, las demás operaciones y
configuraciones siguen pendientes; esto no cierra T08-015.
