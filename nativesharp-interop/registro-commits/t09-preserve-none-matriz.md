# Matriz de firmas escalares preserve_none

Se amplía la llamada directa `preserve_none` a siete funciones con un argumento y retorno del mismo tipo (`float`, `int`, `unsigned int`, `long long`, `unsigned long long`, `double` y `bool`), y una función `void(int*)`. El fixture utiliza funciones C++ de enlace C y comprueba resultados, signo, valores altos, modificación por puntero y ambos valores booleanos; inspecciona que cada llamada conserve `preserve_nonecc`.

La matriz `pruebas-tanda-09-preserve-none-matriz` pasó las ocho combinaciones Clang/NativeSharp de bibliotecas estáticas y DLL en O0–O3 (`A:/temp/nsc-t09-preserve-cpp-escalares-l_aw_ud3`) y dieciséis consumidores precompilados con y sin encabezado (`A:/temp/nsc-t09-preserve-cpp-escalares-precompilado-zj5bin8v`). La prueba negativa conservada rechazó `int(short)` (`A:/temp/nsc-t09-preserve-none-negativo-8e5d1_3k`). La compilación incremental pasó.

Este resultado no acredita combinaciones de firmas distintas, agregados, variádicas ni binding TLS aislado. La ruta indirecta tiene evidencia separada para [una firma mixta C++](t09-preserve-none-indirecta.md) y [cuatro firmas escalares C](t09-preserve-none-callback-matriz.md). T09-003 y la tanda 9 siguen abiertos. Este fixture no acredita recuperación por caché.
