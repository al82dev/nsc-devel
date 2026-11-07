# T08: apuntado const y volatile de TLS atómico unsigned

NSC rechazaba `_Thread_local _Atomic(const unsigned int*)` y `_Thread_local _Atomic(volatile unsigned int*)`. Se permite su proyección conservando los calificadores. Para el apuntado volatile de este global C atómico se propaga el contrato Microsoft a desreferenciación e indexación de `uint` de cuatro bytes y se reconoce en el descenso LLVM.

La primera versión reproducida producía accesos `volatile` simples sin acquire/release; se corrigió antes de registrar el cambio. El control C con Clang y NativeSharp pasó O0–O3 para lectura y escritura directas e indexadas. El IR contiene dos lecturas `atomic volatile` acquire y dos escrituras release de i32; se rechazan la escritura al apuntado const y las conversiones que descartan const o volatile. El fixture completo de atómicos pasó también los controles previos.

Este resultado se limita a los casos descritos y no resuelve los cuatro fallos DLL ni cierra T08. NSLink no se modificó.
