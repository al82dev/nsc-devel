# T08: TLS C atómico sin signo de 32 y 64 bits

Se proyectan `_Thread_local _Atomic(unsigned int)` y `_Atomic(unsigned long
long)` cuando Clang acredita tamaño y alineación de 32 o 64 bits. Los valores
corresponden a `uint` y `ulong`; cargas, escrituras y actualizaciones respetan
orden secuencialmente consistente. El desplazamiento a la derecha utiliza
`lshr` y la división el descenso sin signo. El control negativo se conserva
para `_Atomic(short)`, todavía no proyectado.

La construcción incremental de `nsc` pasó. El control agrupado `atomic/validar.cmd`
pasó en C y NS O0–O3, incluyendo división y desplazamiento con el bit alto
activado en ambos anchos y las pruebas anteriores de tipos firmados. El IR se
comprueba por global y alineación. Registros `A:/nsc-interop-build/tanda08-atomic-unsigned-build.log`
y `A:/nsc-interop-build/tanda08-atomic-unsigned-highbit.log`. Faltan más tipos,
calificadores y usos; esta evidencia no cierra la tanda 8.
