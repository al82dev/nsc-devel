# TLS con el runtime real del proyecto NSC

Una prueba O0 conservó el `runtime.obj` generado por el flujo de ejecutable
de NSC mediante `NSC_KEEP_LINK_TEMPS`. LLD lo enlazó, sin adaptador de Main,
con el objeto del mismo proyecto y la biblioteca C TLS. El ejecutable pasó la
comprobación entre hilos. El enlace se hizo externamente y no demuestra que
la ruta normal de NSC seleccione LLD o configure sus dependencias.

Se prepara una matriz reproducible O0–O3 que conserva cada runtime y confirma
el error `_tls_index` de NSLink antes de enlazar con LLD. El directorio de cada
ejecución permanece en A:; la matriz completa aún no se ha ejecutado. No se
modifican NSLink ni LLVM, y la tanda sigue abierta.
