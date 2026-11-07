# Límite de acceso a TLS con relleno cero

El helper `cppDllTlsAddressHelper` calculaba el límite exclusivamente desde
`EndAddressOfRawData`. Se amplía ese límite con el `SizeOfZeroFill` de la
misma entrada TLS PE, conservando la comprobación inferior y las demás
validaciones. Así el global exportado dentro de la zona inicializada a cero
se calcula con el mismo desplazamiento relativo al inicio de la plantilla.

La reproducción está en `tls-dll-relleno-cero-prueba.md`. La corrección se
registra antes de construir y validar; la validación todavía está pendiente.
No cambia NSLink ni altera los fixtures previamente fallidos.
