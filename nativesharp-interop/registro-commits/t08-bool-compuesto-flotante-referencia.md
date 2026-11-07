# T08-015: referencia flotante de `_Bool` C compuesto

Clang 23.1.0 compiló en C11 O0 para Windows x64 MSVC las funciones
`add_float` y `divide_double` de `BoolCompoundReference.c`. El IR lee el byte
volatile acquire, convierte el bool a `float` o `double`, realiza `fadd` o
`fdiv` y convierte el resultado con `fcmp une` frente a cero antes de escribir
el byte volatile release. La comparación `une` es relevante para conservar el
resultado no cero incluso cuando la operación produce NaN.

IR: `A:/nsc-interop-build/t08-bool-compuesto-flotante-ref.ll`. Esta observación
no demuestra soporte NSC ni cubre todas las opciones y operadores flotantes.
