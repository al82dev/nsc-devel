# T08: aritmética sobre TLS C `_Atomic(int*)`

NativeSharp puede sumar o restar elementos al puntero TLS atómico `int*` e
incrementarlo o decrementarlo. El descenso multiplica el desplazamiento por
el tamaño comprobado de `int`, actualiza la representación con `atomicrmw`
`seq_cst` y devuelve el valor previo o el nuevo según la expresión. Preserva
`volatile`. El caso se limita al puntero a `int` de cuatro bytes ya admitido.

Clang 23.1.0 rechazó en C11 la sintaxis directa `+=` sobre `_Atomic(int*)`.
El control C usa `__c11_atomic_fetch_add/sub` con desplazamientos en elementos;
el IR observado multiplica por el tamaño del tipo apuntado. La primera prueba
usó erróneamente desplazamientos en bytes y se corrigió al examinar ese IR.
Los controles C y NativeSharp pasaron en O0–O3 sobre un arreglo real, incluidos
pre/post incremento y decremento `volatile`; el IR NativeSharp exige las
actualizaciones atómicas `volatile`. También pasó un desplazamiento negativo.
Registros:
`A:/nsc-interop-build/tanda08-atomic-pointer-arithmetic-build.log` y
`A:/nsc-interop-build/tanda08-atomic-pointer-arithmetic-offset.log`.
Faltan otras formas de punteros, tipos apuntados y el gate completo de T08.
