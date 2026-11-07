# T08: contraste del índice TLS de la DLL

Se inspeccionó la DLL del control fallido con `llvm-readobj` y `llvm-objdump`: su directorio TLS contiene `AddressOfIndex` y la función C `tls_zero_read` carga precisamente ese índice antes de acceder al almacenamiento del hilo. El instrumento C existente observó índice 0, datos del hilo distintos de dos valores de la plantilla y un retorno distinto de cero.

Sin modificar el objeto C ni NSC, se enlazó una copia aislada del mismo `api.obj` con `_DllMainCRTStartup` y CRT estático. El mismo instrumento C, reenlazado a esa DLL, observó índice 1, coincidencia de esos dos valores y retorno cero. Salidas: `A:/nsc-interop-build/t08-tls-contraste-crt/noentry-observado.txt` y `crt-observado.txt`.

La diferencia demuestra el comportamiento observado en estas dos imágenes, no identifica por sí sola por qué el cargador no asignó el índice en la primera: el enlace con CRT también cambia importaciones y otras partes de la imagen. Los controles originales se conservan y los cuatro fallos TLS siguen abiertos; no se modificaron NSLink, Clang ni C:.
