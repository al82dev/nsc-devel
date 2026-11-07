# T08: control de aislamiento de punteros atomicos TLS

El hilo NativeSharp asigna los cuatro punteros y un hilo C comprueba sus propios
valores iniciales, los modifica y termina. NativeSharp comprueba que conserva
los suyos. El control requiere compilacion y ejecucion; aun no demuestra aislamiento.
