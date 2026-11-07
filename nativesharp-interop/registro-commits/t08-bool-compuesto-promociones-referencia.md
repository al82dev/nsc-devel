# T08-015: promociones restantes para `_Bool` C compuesto

Clang 23.1.0 compiló `BoolCompoundReference.c` en C11 O0 para Windows x64
MSVC. En las funciones añadidas, bool y enteros de 8 y 16 bits se promueven
a i32; los enteros con signo se extienden con signo y los sin signo con cero.
Para `<<=` con operando `unsigned long long`, el valor `_Bool` se promueve a
i32 y el contador se reduce a ese ancho en el IR. Los accesos observados al
objeto siguen siendo byte volatile acquire/release.

IR: `A:/nsc-interop-build/t08-bool-compuesto-promociones-ref.ll`. Esto no
valida NSC ni atribuye resultado definido en C a contadores inválidos.
