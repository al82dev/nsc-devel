# Dictionary: regresión dirigida en O0 y O2

La prueba `DictionaryStringUpdate` verifica la actualización de cadenas del SDK
y la destrucción al terminar el programa. Su ejecutor permanente ahora compila
y ejecuta el mismo proyecto en O0 y O2, sin modificar el ejemplo.

El 11 de noviembre de 2026 se ejecutó el script CMake dirigido con el compilador
`A:/nsc-fixes-evidencia/build/bin/nsc.exe`, el runtime del build aislado y
`nslink.exe` resuelto por nombre. El registro está en
`A:/nsc-fixes-evidencia/dictionary-o0-o2-integrated-20261111.log`. Los dos
ejecutables generados se ejecutaron también por separado y ambos devolvieron 0.

La invocación original del script perdió su código de salida en la captura de
la herramienta; el registro quedó vacío y se verificaron los dos ejecutables
generados. Esta evidencia cubre este proyecto, no todos los usos de Dictionary.
