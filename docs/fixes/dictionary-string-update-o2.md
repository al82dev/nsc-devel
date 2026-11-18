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

La primera comprobación manual de los dos ejecutables se inició por error desde
el directorio de resultados, en vez de desde el repositorio indicado por los
lineamientos; se conserva como ejecución no conforme. Se repitieron ambas desde
`A:/nsc-devel-fixes` y devolvieron 0. La ejecución inicial del script CMake se
había iniciado desde el repositorio y su código de salida no se conservó, como
se indica arriba.

El script CMake original invocaba además un CMake hijo por ruta absoluta; esa
ejecución es no conforme. Tras corregirlo se repitió desde el repositorio con
las herramientas por nombre y pasaron O0 y O2. Evidencia:
`A:/nsc-fixes-evidencia/cmake-runner-corrections-20261112.json` y
`runner-correction-dictionary-20261112.log`.
