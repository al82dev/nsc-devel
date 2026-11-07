# Controles de proyección TLS C

La matriz preparada comprueba que LLVM declare `shared_tls` como global
externo `thread_local`, ejecuta lectura y escritura cruzadas con C y verifica
aislamiento entre hilos en O0–O3. Un global TLS `volatile` todavía fuera del
alcance debe rechazarse en lugar de perder su calificador. Las pruebas están
pendientes de construcción y ejecución agrupadas.
