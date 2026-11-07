# Alcance: TLS de DLL C en productor precompilado

Se comprobará la combinación de dos rutas ya observadas por separado: acceso
directo NativeSharp al TLS de DLL C y consumo de biblioteca NativeSharp
precompilada sin volver a importar el header. La DLL de la prueba contiene
importaciones reales y no sustituye los controles C con fallo conocidos.
