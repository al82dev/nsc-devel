# T08-015: referencia C de división y desplazamientos sobre `_Bool`

Clang 23.1.0 compiló `BoolCompoundReference.c` para Windows x64 MSVC, C11 y
O0. Con operandos positivos válidos, el IR de `/=`, `%=`, `<<=` y `>>=`
lee el byte volatile con acquire, lo promueve a i32, aplica respectivamente
`sdiv`, `srem`, `shl` y `ashr`, convierte el resultado a cero/no cero y
escribe el byte volatile con release. Salida:
`A:/nsc-interop-build/t08-bool-compuesto-shift-ref.ll`.

Esto establece la referencia para esos ejemplos, no valida NSC ni atribuye
semántica definida a divisores cero o desplazamientos inválidos en C.
