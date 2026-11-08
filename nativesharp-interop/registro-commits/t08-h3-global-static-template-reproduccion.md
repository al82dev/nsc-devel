# T08: reproducción de global C static en plantilla precompilada

El encabezado define una global C `static` inicializada a 40 y una función `static inline` que la lee. La plantilla NativeSharp incrementa la global y llama a esa función; el consumidor separado debe obtener 42 sin recibir el encabezado. Esta condición comprueba que ambos accesos usen el mismo almacenamiento.

Con el build `939c2c8c8ebd33c6` y O0, el productor emitió la biblioteca, pero el consumidor falló con `unknown identifier 'fixture_counter'`. Evidencia: `A:/nsc-interop-build/tanda08-global-static-template.log`. El fixture documenta un fallo y no se integra todavía en la batería general.
