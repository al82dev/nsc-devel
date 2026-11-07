# T08-004: cobertura IR de cada puntero atómico

El control identifica las variables de las cargas y comprueba que las cinco
también tengan una escritura o actualización atómica. Evita considerar que
dos instrucciones genéricas son evidencia suficiente para toda la matriz.
