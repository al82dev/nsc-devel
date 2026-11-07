# T08: contraste de importación en DLL TLS sin punto de entrada

Sobre el mismo `api.obj` y el mismo `inspect.obj` usados en el contraste anterior, se enlazó una DLL con `/noentry`, `/nodefaultlib` e idéntico objeto C, añadiendo `/include:__imp_GetCurrentThreadId` y la biblioteca de importación de Kernel32. No se modificó el código C, NSC ni NSLink.

`llvm-readobj` confirmó `AddressOfEntryPoint: 0` y una tabla de importaciones presente. En Windows `10.0.28000.3231`, el instrumento observó índice TLS 1, coincidencia de dos posiciones con la plantilla y retorno cero de `tls_zero_read()`. El control anterior sin importaciones observó índice 0, datos diferentes y retorno no cero. La salida está en `A:/nsc-interop-build/t08-tls-contraste-import/observado.txt`; la imagen y los objetos se conservan junto a ella.

La comparación muestra una diferencia reproducida al forzar una importación, con el punto de entrada ausente en ambas imágenes. No demuestra el mecanismo interno del cargador ni que toda DLL sin importaciones sea inválida. No se sustituyeron los controles originales y los fallos de la validación integral siguen abiertos.
