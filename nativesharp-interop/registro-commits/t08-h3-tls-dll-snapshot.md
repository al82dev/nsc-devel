# Tanda 8: conservar importación efectiva de TLS C

El estado de importación de una global TLS C puede proceder de la referencia a
una DLL, aunque la declaración de Clang no tenga el atributo `dllimport`.
El snapshot interno conserva ese estado efectivo y lo restituye al proyectar
la declaración, manteniéndolo separado del atributo de la declaración original.
Se rechazan estados incompatibles con el enlace interno o con el atributo de la
declaración. La versión del registro interno de globales cambia a 2; este
catálogo de dependencias todavía no se publica en NSMETA.

Se construyó incrementalmente `nsc_cpp_c_identity_tests` y su ejecución pasó
con Clang 23.1.0. Las comprobaciones cubren captura, codificación, decodificación
y proyección del indicador. La proyección TLS se comprueba con un snapshot
sintético de una declaración real; no constituye una prueba de extremo a extremo
con una DLL real. La ruta de producción y la identidad de la DLL siguen pendientes.
La compilación mostró advertencias de inicializadores en código existente.
