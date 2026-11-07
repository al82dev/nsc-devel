# Matriz de DLL con TLS y optimizaciones

El control de DLL de `lld/validar-sin-entorno.cmd` se amplía de O0 a O0–O3
para productor y consumidor NativeSharp. En cada nivel se inspeccionan las
exportaciones, se comprueba la ausencia del accesor C inexistente y se
ejecutan lectura, escritura y aislamiento por hilo. La preparación no altera
los controles anteriores ni NSLink. Esta matriz aún está pendiente de
ejecución y no cierra la tanda 8.
