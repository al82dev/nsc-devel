# T08: asignaciones bit a bit sobre TLS C atómico

El descenso de un global C `_Thread_local _Atomic(int)` rechazaba `&=`, `|=` y
`^=`. Para el caso ya admitido de `int` alineado a 4 bytes, ahora genera
`atomicrmw` `and`, `or` o `xor` con orden secuencialmente consistente. El valor
de la expresión se calcula con el valor anterior devuelto por esa instrucción;
se conserva `volatile` cuando corresponde. El resto de tipos y operaciones no
admitidas siguen sin declararse compatibles. Los valores del enum LLVM se
contrastaron con `llvm/include/llvm-c/Core.h` de LLVM 23.1.0.

La construcción incremental del objetivo `nsc` terminó con código 0. La prueba
`pruebas-tanda-08-tls-c/atomic/validar.cmd` terminó con código 0: comprueba el
valor de cada expresión en el control C y en NativeSharp O0–O3, y verifica que
ambos IR contienen operaciones secuencialmente consistentes asociadas al TLS.
Registros en `A:/nsc-interop-build/tanda08-atomic-bitwise-build.log` y
`A:/nsc-interop-build/tanda08-atomic-bitwise-validar.log`. Esta evidencia no
representa la validación completa de la tanda.
