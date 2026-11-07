# Contraste de carga dinámica de DLL TLS sin importaciones

`validar-escalar-carga-dinamica.cmd` construye la misma DLL de escalares sin
importaciones y la carga explícitamente con `LoadLibraryA`; el consumidor C
obtiene `tls_zero_read` mediante `GetProcAddress`. El módulo y la función se
obtuvieron, pero la comprobación de cero devolvió 1. La variante cargada como
dependencia de arranque también falló. Cambiar el orden de `kernel32.lib` y
`api.lib` en el enlace del consumidor no alteró ese resultado.

Esto limita la hipótesis a la configuración de la DLL o de su inicialización,
pero no identifica todavía la causa. No se modifica el consumidor NativeSharp,
NSLink ni la DLL real de terceros; ambos controles fallidos se conservan.
