# Tanda 08: uso real de global enum conflictivo

- Se prepara una compilación que usa el global C `hidden_global` mientras existe un struct NativeSharp `HiddenState` homónimo del enum C.
- Se exige el rechazo específico de la proyección del global, complementando la prueba de sombra que debe compilar. Pendiente de ejecución agrupada.
