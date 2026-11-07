# Tanda 8: validación agrupada de emisión directa

La batería `nativesharp-interop/validar-tanda-08.cmd` terminó con código 1. El registro está en `A:/nsc-interop-build/tanda08-validacion-emision-auto.log`. El nuevo control de emisión directa terminó correctamente para TLS, bitcode, prioridad de selección y caché.

Se reprodujeron los cuatro fallos anteriores: enlace del consumidor C de TLS de DLL, las dos variantes de escalares sin importaciones y el control C de carga dinámica. No se quitaron ni alteraron esos controles; la tanda continúa abierta y no acredita la emisión directa de DLL.
