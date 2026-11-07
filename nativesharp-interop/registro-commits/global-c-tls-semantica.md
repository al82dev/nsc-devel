# Proyección semántica de global C TLS entero

El control C del fixture pasó; NSC rechazó `shared_tls` por no proyectar TLS.
Se permite exclusivamente un global C externo TLS, entero firmado de 32 bits,
mutable, no volatile y sin `dllimport`, y se transporta el indicador al estado
de namespace. Otros tipos y contratos TLS permanecen rechazados. Aún faltan el
transporte a MIR, la declaración LLVM y la validación agrupada; el cambio por
sí solo no acredita ejecución correcta.
