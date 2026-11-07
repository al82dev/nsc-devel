# Tanda 8: validación agrupada de proyectos nuevos

Se ejecutó `nativesharp-interop/validar-tanda-08.cmd` con el compilador de desarrollo de A:, tras las correcciones de selección y serialización de enlazador. El registro completo está en `A:/nsc-interop-build/tanda08-validacion-proyecto-nuevo.log`; el comando terminó con código 1.

Pasaron dentro de la batería los controles de bitcode con y sin TLS, el sidecar parcial y el nuevo proyecto con operaciones de paquetes y selecciones explícitas. Se reprodujeron los cuatro fallos previos: enlace del consumidor C de TLS de DLL, ejecución de escalares sin importaciones, ejecución de escalares con entry y sin importaciones, y control C de carga dinámica. No se modificaron ni desactivaron esos controles. La tanda continúa abierta.
