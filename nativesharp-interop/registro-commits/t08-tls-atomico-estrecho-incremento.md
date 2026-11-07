# T08: incremento y decremento de TLS C atómico estrecho

El control C y el proyecto NativeSharp comprueban preincremento, postincremento,
predecremento y postdecremento de `_Atomic(short)`, con valores negativos y
cero, además de los mismos accesos sobre `volatile _Atomic(short)`. El IR de
NativeSharp debe contener al menos cuatro intercambios `seq_cst` sobre cada
símbolo, con `volatile` en el segundo y alineación 2.

El control agrupado `atomic/validar.cmd` pasó en O0–O3 sin reconstruir NSC;
registro `A:/nsc-interop-build/tanda08-atomic-narrow-inc-final.log`. Este
resultado cubre los casos preparados; faltan otras combinaciones y el gate
completo de T08. No se cambió NSLink.
