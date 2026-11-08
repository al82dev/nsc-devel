# Macros dependientes del tiempo en la huella de Clang

El contenido de un encabezado no cambia necesariamente cuando se expande
`__DATE__`, `__TIME__` o `__TIMESTAMP__`. Esas macros pueden afectar código o
directivas que el compilador procesa, por lo que la sesión devuelve una huella
ausente si observa su expansión. Así el consumidor futuro no puede tratar esa
huella como una identidad segura. Detectar una macro del mismo nombre definida
por el usuario también deshabilita la huella de forma conservadora.

La construcción incremental del objetivo `nsc_cpp_session_tests` terminó y la
prueba de sesión comprobó las tres macros con código de salida 0. La compilación
mostró avisos existentes sobre inicializadores incompletos.

Este cambio no activa la caché
de referencias C/C++; siguen pendientes la identidad completa y su validación.
