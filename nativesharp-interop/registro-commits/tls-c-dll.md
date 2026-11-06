# Aislamiento TLS dentro de DLL C

Se prepara un caso independiente que enlaza la DLL con lld, conserva su
directorio TLS y llama desde NativeSharp una función que comprueba aislamiento
entre dos hilos. No prueba importar directamente un global TLS ni soporte de
TLS estático de NSLink. Pendiente construir y ejecutar.

Clang y lld generaron una DLL x64 con `_tls_used` de `libcmt.lib`, sin entry
point explícito. `llvm-readobj --coff-tls-directory` confirmó un directorio
TLS con índice y datos iniciales; el ejecutable NS enlazado mediante la
biblioteca de importación llamó la función y comprobó aislamiento entre dos
hilos en O0–O3. El script alcanzó `TLS DLL COMPLETO` con código 0.
Se conserva la limitación: no se probó acceso directo a un global TLS y el
caso de objeto C estático en NSLink continúa sin resolver `_tls_index`.
