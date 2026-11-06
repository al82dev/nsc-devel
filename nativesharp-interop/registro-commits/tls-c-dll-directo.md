# Acceso directo a TLS exportado por una DLL C

Se prepara un global TLS exportado, una referencia C construida por Clang y
un consumidor NativeSharp que lee y escribe la variable y comprueba aislamiento
entre hilos. La prueba registra relocaciones y no modifica NSLink. Pendiente
construir y diagnosticar cada etapa; no acredita todavía el acceso directo.
