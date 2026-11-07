# T08: HIR de puntero atómico global a float

La proyección preparada para `_Atomic(float*)` requiere que HIR conserve la
alineación de ocho bytes del almacenamiento del puntero, igual que los otros
punteros atómicos de 64 bits. Se agrega esa clave a la ruta existente. El
cambio aún no se ha construido ni probado y requiere descenso LLVM; no modifica
NSLink ni acredita soporte por sí solo.
