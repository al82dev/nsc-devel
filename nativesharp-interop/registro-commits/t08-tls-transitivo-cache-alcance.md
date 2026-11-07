# Caché al cambiar una dependencia transitiva

Se delimita una prueba que reconstruye el productor precompilado y cambia su
dependencia C de COFF sin TLS a COFF con TLS. El consumidor debe cambiar de
enlazador y ejecutar el nuevo resultado, incluso con la caché activa. El caso
no presupone un fallo y no cambia los controles de DLL C.
