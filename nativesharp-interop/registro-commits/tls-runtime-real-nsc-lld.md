# TLS con el runtime real del proyecto NSC

Una prueba O0 conservó el `runtime.obj` generado por el flujo de ejecutable
de NSC mediante `NSC_KEEP_LINK_TEMPS`. LLD lo enlazó, sin adaptador de Main,
con el objeto del mismo proyecto y la biblioteca C TLS. El ejecutable pasó la
comprobación entre hilos. El enlace se hizo externamente y no demuestra que
la ruta normal de NSC seleccione LLD o configure sus dependencias.

La matriz `validar-runtime-real.cmd` pasó O0–O3 con NSC build
`a02f3209efef196f`; conservó cada runtime y confirmó el error `_tls_index`
de NSLink antes del enlace externo con LLD. Evidencia en
`A:/nsc-interop-build/tanda08-tls-runtime-real-18480-12252`. Esto no prueba
todavía selección ni enlace automáticos en NSC. No se modificaron NSLink ni
LLVM, y la tanda sigue abierta.
