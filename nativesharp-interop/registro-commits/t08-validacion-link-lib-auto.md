# Tanda 8: validación agrupada de `--link-lib`

La batería `nativesharp-interop/validar-tanda-08.cmd` terminó con código 1. El registro está en `A:/nsc-interop-build/tanda08-validacion-link-lib-auto.log`. El nuevo control `--link-lib` pasó dentro de la batería para ejecutable y DLL O0–O3, selección explícita, bitcode y consumidor NativeSharp O0.

Continuaron fallando los mismos cuatro controles originales de DLL C: enlace del consumidor C, dos variantes de escalares sin importaciones y carga dinámica C. Ninguno se eliminó ni modificó; T08 permanece abierta.
