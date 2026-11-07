# Matriz de DLL con TLS y optimizaciones

El control de DLL de `lld/validar-sin-entorno.cmd` se amplía de O0 a O0–O3
para productor y consumidor NativeSharp. En cada nivel se inspeccionan las
exportaciones, se comprueba la ausencia del accesor C inexistente y se
ejecutan lectura, escritura y aislamiento por hilo. La preparación no altera
los controles anteriores ni NSLink. La matriz pasó en O0–O3 con NSC build
`b910fa13ea6d5855`; también pasaron el ejecutable TLS, la biblioteca estática
y el enlace incremental de ese fixture. La tanda 8 permanece abierta.
