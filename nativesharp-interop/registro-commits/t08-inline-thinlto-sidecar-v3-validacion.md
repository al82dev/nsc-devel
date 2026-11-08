# T08-003: validación de sidecars con objetos nativos

La construcción incremental de `nsc` y la suite de ThinLTO terminó con código
0. El compilador de desarrollo usado por los controles de interoperabilidad fue
`c1d4e389ef03ddc9`.

La primera ejecución de la suite informó tres fallos de su fixture general:
capturaba archivos de subproyectos con un glob recursivo y confundía 10
summaries con 1 al comparar un prefijo. Corregido en `4f75a3d25`, se recompiló
solo el objetivo de prueba y la ejecución agrupada pasó **41 comprobaciones**,
incluidos el round-trip v3 binario, rechazo de longitud inválida autenticada,
objeto vacío y escritura/lectura v2 sin objetos.

La matriz inline pasó mediante `call` en O0–O3. En O1–O3 exigió sidecars para
dos productores y `thinlto-libraries: 2` al consumirlos; comprobó lectura y
escritura y la independencia de sus estados internos. O0 usa código normal y
elimina sidecars obsoletos. El control de llamada desde inline C a un símbolo
NativeSharp pasó O0–O3 en el ejecutable directo.

Se conserva visibilidad de definiciones si hay objetos nativos, en lugar de
extraer todavía sus referencias como raíces. Faltan controles de corrupción y
de referencias a NativeSharp en sidecars reales, y la validación integral de
la tanda 8. No se modificó NSLink ni la instalación de C:.
