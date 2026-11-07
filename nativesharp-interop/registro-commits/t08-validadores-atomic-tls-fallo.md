# T08: propagación de fallos en globales atómicas y TLS

Los validadores de globales y TLS atómicos se actualizan para emitir un fallo
explícito al detectar una compilación o diagnóstico incorrectos. El destino
común presenta los registros y devuelve uno; las subrutinas de consumidores
conservan la señal de error hacia su llamador. Pendientes controles normales y
fallos inducidos en copias aisladas.
