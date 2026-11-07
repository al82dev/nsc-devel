# Tanda 8: validación agrupada de DLL directa

La ejecución de `nativesharp-interop/validar-tanda-08.cmd` terminó con código 1; el registro está en `A:/nsc-interop-build/tanda08-validacion-dll-emision-auto.log`. Dentro de la batería pasó el nuevo control de la DLL NativeSharp que enlaza TLS C, con directorio TLS y lectura, escritura y segundo hilo en O0–O3.

Persistieron los cuatro fallos de los controles originales de DLL C: enlace directo del consumidor C, dos variantes de escalares sin directorio de importación y carga dinámica C. Estos casos no fueron eliminados ni sustituidos. La tanda sigue abierta.
