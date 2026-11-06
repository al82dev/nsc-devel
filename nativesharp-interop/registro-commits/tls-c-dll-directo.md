# Acceso directo a TLS exportado por una DLL C

Se prepara un global TLS exportado, una referencia C construida por Clang y
un consumidor NativeSharp que lee y escribe la variable y comprueba aislamiento
entre hilos. La prueba registra relocaciones y no modifica NSLink. Pendiente
construir y diagnosticar cada etapa; no acredita todavía el acceso directo.

Clang 23 con destino Windows MSVC rechazó explícitamente combinar
`__declspec(thread)` con `dllexport` y, por separado, con `dllimport`. Se
prepara una segunda observación con declaración TLS sin esos atributos y
exportación por opción del enlazador; aún no se sabe si puede enlazar ni
ejecutar y no se ha modificado NSLink.

La segunda DLL enlazó y el consumidor C compiló, pero lld no pudo resolver
`tls_exported`: el objeto pide `IMAGE_REL_AMD64_SECREL` y la biblioteca de
importación publica `__imp_tls_exported`. El primer enlace también reportó
dependencias estáticas de excepciones; al añadir `libcpmt`, `libvcruntime` y
`libucrt` el único error restante fue `tls_exported`. La prueba NS por separado
se detuvo con `thread-local C/C++ global is not yet projected`. No se concluye
que todas las técnicas de TLS entre DLL sean imposibles; esta ruta no enlaza.
