# Ampliación del control de arreglos TLS

Se agregan `int[3]` e `int[2][3]` a los controles C y NativeSharp, con
comprobación de inicialización, escritura y aislamiento entre hilos. Se
conserva `int[2]`. Los nuevos casos esperan validación agrupada en estático
y DLL; no representan por sí solos todas las dimensiones y tamaños. El
control C pasó y NSC `7aad885cf958259e` rechazó los dos nuevos globales.
