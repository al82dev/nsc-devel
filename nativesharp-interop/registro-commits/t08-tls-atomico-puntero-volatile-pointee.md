# T08: puntero TLS atómico a dato volatile

`_Thread_local _Atomic(volatile int*)` se rechazaba al importar por una condición de `atomicPointerTls`. Se retira esa exclusión para `int`: la proyección existente conserva el calificador del dato apuntado, y las rutas existentes generan sus accesos volatile y evitan la conversión implícita a un puntero sin calificar.

El control en C con Clang 23.1.0 y el programa NativeSharp leen y escriben el dato; NSC se comprueba en O0–O3. El IR del perfil probado contiene escritura atomic volatile release y lectura atomic volatile acquire, y se comprueba el rechazo de la conversión que descartaría volatile. Estas pruebas son dirigidas: no cierran la tanda 8 ni acreditan otras variantes de puntero. NSLink no se modifica.
