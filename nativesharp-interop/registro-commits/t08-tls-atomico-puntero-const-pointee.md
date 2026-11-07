# T08: puntero TLS atómico a dato constante

La importación de `_Thread_local _Atomic(const int*)` se rechazaba antes de proyectar el tipo. La condición de `atomicPointerTls` excluía el `const` del dato apuntado, aunque la proyección ya transporta ese calificador y el análisis semántico impide descartarlo o escribir a través del puntero. Se retiró únicamente esa exclusión; el almacenamiento sigue siendo atómico y el caso con dato apuntado `volatile` continúa rechazado.

La prueba dirigida contrasta lectura y aritmética con Clang C11 y NSC, y comprueba que NSC rechaza la escritura y la conversión implícita a puntero mutable. Se valida con el compilador de desarrollo de A: y O0–O3. No acredita la tanda 8 completa ni cambia NSLink.
