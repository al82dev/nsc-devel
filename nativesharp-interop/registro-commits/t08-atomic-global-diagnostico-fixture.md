# T08: fixture de diagnóstico para global C atómico no TLS

Se prepara un global C `_Atomic(int)` sin TLS con lectura y actualización
compuesta desde NativeSharp, contrastadas mediante una función C. El fixture
aún no se ejecuta; no demuestra soporte ni identifica por sí solo la causa de
un eventual rechazo. No modifica el compilador ni NSLink.
