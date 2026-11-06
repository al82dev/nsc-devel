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
