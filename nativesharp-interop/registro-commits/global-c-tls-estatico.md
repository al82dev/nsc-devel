# Global C TLS estático desde NativeSharp

Se prepara una biblioteca con `extern __declspec(thread) int shared_tls` y
consumidores C y NativeSharp. Ambos deben leer el valor inicial, escribirlo,
compararlo con C y comprobar el aislamiento de un segundo hilo. LLD y el CRT
estático se declaran explícitamente; la prueba no cubre TLS de DLL. La matriz
O0–O3 está pendiente; no se ha implementado aún la proyección del global.
