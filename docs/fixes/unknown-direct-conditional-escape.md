# Procedencia de punteros en expresiones condicionales

Al pasar un puntero seleccionado mediante `?:` a una función NativeSharp precompilada cuyo contrato de escape es desconocido, se perdía el segundo origen posible. Una función intermedia podía recibir una dirección local y reenviarla sin diagnóstico. También se aceptaba una selección directa entre una dirección global y una local.

El análisis de dependencias recorre la expresión del argumento y sus alias locales para vincular los parámetros de puntero con el contrato del destinatario. La clasificación de valores de puntero reúne los orígenes de ambas ramas de una expresión condicional, incluidas selecciones anidadas. El recorrido de dependencias es conservador: puede atribuir escape a un parámetro presente en la expresión aunque no contribuya al valor resultante.

La prueba previa de `nsc.ns3.t06.escape-summaries` registró un fallo de 30 comprobaciones en `A:\nsc-fixes-evidencia\unknown-direct-conditional-before.log`; el compilador aceptaba la llamada indirecta. Al agregar un control de selección directa se observó otro fallo, antes de extender la clasificación del valor. Ningún ejemplo inseguro se ejecutó.

Con `A:\nsc-fixes-evidencia\unknown-direct-conditional-after.cmd`, la compilación incremental y la prueba dirigida pasan 32 comprobaciones, incluyendo rechazo del escape local y aceptación de una selección global. El script `A:\nsc-fixes-evidencia\unknown-direct-conditional-controls.cmd` pasa las pruebas dirigidas de escape y vida de vistas (40 y 31 comprobaciones), la selección de pruebas de punteros públicos y la integración dirigida de Arc con el SDK binario aislado. Los registros llevan el prefijo `unknown-direct-conditional-` en la carpeta de evidencia.

Estas pruebas se ejecutaron con el árbol y el SDK aislados de trabajo, que incluyen cambios aún no terminados. No constituyen validación completa de todas las expresiones posibles ni cierre de la campaña; siguen pendientes los casos de tuplas con vistas precompiladas y los demás trabajos documentados en el estado de la campaña.
