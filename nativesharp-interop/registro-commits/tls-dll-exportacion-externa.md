# Exportación de un global TLS C importado

La DLL NativeSharp del fixture `lld/validar-sin-entorno.cmd` usa un global C
TLS mediante una biblioteca estática. Con el compilador previo, LLD rechazó
el enlace al no encontrar `__nsc_get_shared_tls`: `collectDynamicStateExports`
lo solicitaba para todo estado público, aunque el estado importado de C no
genera un accessor NativeSharp.

Se excluyen los estados C/C++ externos de esa lista de exportaciones; los
estados NativeSharp mantienen su condición anterior. No se cambia NSLink.
Con NSC build `b910fa13ea6d5855`, la construcción incremental y
`lld/validar-sin-entorno.cmd` pasaron. La DLL conservó la exportación de
lectura NativeSharp, no exportó el accesor externo inexistente y un
consumidor NativeSharp ejecutó lectura, escritura y aislamiento TLS entre
hilos. La selección existente de enlazador y la matriz TLS O0–O3 también
pasaron. Esto no cierra la tanda 8.
