# Campo de puntero a entero constante en registro C++ SysV

Se admite, en la validación de la disposición del registro por copia, un campo `const int*` cuya identidad y representación coinciden con lo observado por Clang. La excepción se limita a un nivel de puntero y al entero firmado de 32 bits observado; el rechazo anterior permanece para las rutas escalares independientes, `volatile`, punteros anidados calificados y arreglos de punteros.

El fixture `pruebas-tanda-09-sysv-cpp-copia-const-puntero` prueba `{int,double,const int*}` con retorno `{float,float,int}`, comprobando el dato apuntado y la copia. El control Clang 23.1.0 previo pasó en O0–O3 con bibliotecas estáticas y DLL; NSC inicialmente rechazaba la firma. Tras el cambio, la misma matriz de ocho comparaciones C++/NativeSharp pasó en `A:/temp/nsc-t09-sysv-cpp-copia-puntero-p2answrn`. También pasaron los dieciséis consumidores precompilados con y sin encabezado en `A:/temp/nsc-t09-sysv-cpp-copia-puntero-precompilado-vdivliap`. Se validaron llamadas directas y mediante puntero.

La compilación incremental y la prueba `nsc.cpp-interop.regcall-record-plan` pasaron, incluyendo los controles de una grafía incoherente y `volatile`. Este caso no acredita otras combinaciones de calificadores ni cierra T09-003 o la tanda 9. Permanecen las restricciones D23 y D26.
