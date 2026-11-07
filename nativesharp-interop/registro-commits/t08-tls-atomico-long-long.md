# T08: TLS C atómico `long long`

NSC rechazaba el global C `_Thread_local _Atomic(long long)`. Se admite el tipo
firmado de 64 bits cuando Clang informa tamaño de 64 y alineación de al menos
64 bits. La proyección conserva `const` y `volatile`; HIR registra alineación
8 y LLVM usa cargas, escrituras y operaciones atómicas secuencialmente
consistentes de ese ancho. Las asignaciones compuestas convierten el operando
al tipo de destino; los desplazamientos usan el normalizador existente.
Siguen rechazándose los tipos atómicos aún no proyectados: el control negativo
usa ahora `_Atomic(unsigned long long)`.

La construcción incremental de `nsc` y `atomic/validar.cmd` pasaron. El control
C y NativeSharp O0–O3 comprueban valores mayores a 32 bits, asignación, suma,
multiplicación, desplazamiento y división; el IR exige carga, escritura,
`atomicrmw` y `cmpxchg` de alineación 8. También pasó la matriz previa de 32
bits. Registros `A:/nsc-interop-build/tanda08-atomic-wide-build.log` y
`A:/nsc-interop-build/tanda08-atomic-wide-validar.log`. No cubre todavía todas
las operaciones, calificadores, concurrencia o formas de publicación de 64 bits.
