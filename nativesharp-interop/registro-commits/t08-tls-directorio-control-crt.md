# Tanda 8: contraste TLS con arranque estándar del CRT

El consumidor C `probe_tls_crt_control.c` usa `main`, `LoadLibraryA` y `GetProcAddress`; se enlazó con CRT estático y LLD. El mapa identifica `mainCRTStartup` en RVA `0x12f0`, coincidente con `AddressOfEntryPoint` en las cabeceras PE. El mismo ejecutable devolvió 8 con la DLL sin directorio de importación y 0 con la copia cuya única diferencia es un descriptor de importación vacío. La salida y archivos de diagnóstico están bajo `A:/nsc-interop-build/tls-abi/crt-control`.

Esta comparación indica que el comportamiento observado no depende solo del punto de entrada sin CRT del control anterior. No demuestra el mecanismo interno de Windows ni constituye una corrección de NSC. No se alteró la DLL original ni se desactivó ningún control; T08 sigue abierta.

Para reproducir, ejecutar el fixture `validar-nsc-escalares.cmd`, que deja sus artefactos aun cuando falla; después `probe_tls_import_directory.py` para generar la copia experimental y `probe_tls_crt_control.cmd` para construir y ejecutar el control.
