# Control de prototipos C en la sesión de Clang

La prueba de sesión contrasta declaraciones C `f()` y `f(void)` en la captura y la proyección semántica, comprobando que solo la primera admita argumentos no fijados. La ABI de llamadas reales se comprueba con el fixture de interoperabilidad.
