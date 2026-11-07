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

El control C pasó tras agregar esas bibliotecas. Con NSC build
`2c5d0967206b6e62`, el consumidor NativeSharp falló en O0 con
`thread-local C/C++ global is not yet projected` para lectura y escritura de
`shared_tls`. Se preparó la propagación semántica y LLVM para el caso entero
C de 32 bits; todavía debe validarse con una nueva construcción.

Con NSC build `1f3bce0dac388677`, el control C y el consumidor NativeSharp
pasaron O0–O3: verificaron valor inicial, escritura y lectura cruzadas y
aislamiento de otro hilo. LLVM emitió `external thread_local global i32` y
el global TLS `volatile`, fuera de este alcance, fue rechazado. LLD y el CRT
estático se declararon explícitamente; no se probó TLS directo de DLL.
