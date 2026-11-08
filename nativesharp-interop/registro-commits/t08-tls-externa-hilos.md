# Aislamiento entre hilos de TLS C externa compartida

Dos plantillas precompiladas usan la misma global C `_Thread_local`. El hilo
principal modifica el valor; un segundo hilo creado con `CreateThread` ejecuta
un callback NativeSharp, observa su valor inicial, lo modifica y comprueba que
ambas plantillas ven el mismo cambio. Al terminar, el hilo principal conserva su
propio valor.

El consumidor compiló y ejecutó en O0 sin los encabezados de los productores,
con LLD y el arranque TLS existente. Se reproduce con
`externa-dos-bibliotecas/validar.cmd tls-hilos`. También pasó la variante TLS
original. El control no acredita DLL, otros tipos ni otras optimizaciones.
