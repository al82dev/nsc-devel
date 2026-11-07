# Exportación de un global TLS C importado

La DLL NativeSharp del fixture `lld/validar-sin-entorno.cmd` usa un global C
TLS mediante una biblioteca estática. Con el compilador previo, LLD rechazó
el enlace al no encontrar `__nsc_get_shared_tls`: `collectDynamicStateExports`
lo solicitaba para todo estado público, aunque el estado importado de C no
genera un accessor NativeSharp.

Se excluyen los estados C/C++ externos de esa lista de exportaciones; los
estados NativeSharp mantienen su condición anterior. No se cambia NSLink.
La construcción y validación agrupadas están pendientes; no se afirma aún
que la DLL ni la tanda estén listas.
