# TLS estático declarado con C11

Se generó una copia del fixture `static-global` cambiando únicamente
`__declspec(thread)` por `_Thread_local` en la declaración y la definición.
Con NSC build `b151b46f0be04227` y LLD seleccionado explícitamente desde A:,
el control C pasó; NativeSharp leyó y escribió el global y verificó su
aislamiento entre hilos en O0–O3. La prueba de arreglo incompleto existente
también conservó su rechazo. No se cambió el fixture fuente ni NSLink.

El contraste pasó con el generador temporal y también mediante
`static-global/validar-c11.cmd` desde el repositorio. La nueva entrada
en el validador conserva todas las pruebas anteriores. Este resultado no
cierra T08-004 ni demuestra selección automática de LLD.
