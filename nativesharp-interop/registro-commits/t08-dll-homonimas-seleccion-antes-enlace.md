# Selección de alias independiente del enlace

La selección de alias TLS se extrajo de la comprobación del PE a `selectCppTlsRuntimeAliases`. Esta operación ya puede invocarse antes del enlace; la comprobación posterior sigue rechazando importaciones directas o transitivas ambiguas. El cambio conserva el comportamiento anterior y prepara la generación de importaciones con el nombre empaquetado.

La construcción incremental de `nsc` en `A:\nsc-interop-build\tanda1-ON` terminó correctamente. `validar-mismo-nombre.cmd` pasó con optimización O0: los controles C, el consumidor NativeSharp y el diagnóstico de importación directa. La generación de la biblioteca de importación y la validación de las importaciones directas siguen pendientes; no se considera terminado el apartado T08-002-09m7c1d2b.
