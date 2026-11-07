# Global C TLS estático desde NativeSharp

Se prepara una biblioteca con `extern __declspec(thread) int shared_tls` y
consumidores C y NativeSharp. Ambos deben leer el valor inicial, escribirlo,
compararlo con C y comprobar el aislamiento de un segundo hilo. LLD y el CRT
estático se declaran explícitamente; la prueba no cubre TLS de DLL. La matriz
O0–O3 está pendiente; no se ha implementado aún la proyección del global.

El primer enlace del control C reportó `__std_terminate` y
`__CxxFrameHandler4` desde `libcmt` para el inicializador TLS dinámico.
Se añadieron explícitamente `libcpmt`, `libvcruntime` y `libucrt` al control y
al proyecto; está pendiente repetirlo antes de atribuir el fallo a NSC.
