# Candidato: FnOnce en parámetros de funciones de namespace

Estado: corrección parcial integrada y validada con el compilador autorizado.
Los sondeos previos invocaron copias de `nsc.exe` por ruta en A sin autorización
expresa; permanecen como evidencia histórica no conforme, no como pruebas aprobadas.

Los sondeos históricos sugieren que la ruta de función de namespace acepta dobles invocaciones
que el reconocimiento de parámetros directos ya integrado para métodos puede detectar. Los
ejemplos potencialmente inseguros se limitaron a análisis; los controles seguros de una llamada
y `Fn` se ejecutaron en O0/O2, pero esas ejecuciones también son no conformes por la ruta del
compilador. Las salidas históricas están en `A:\nsc-fixes-agent1-temp\fnonce-namespace-probe`,
`fnonce-namespace-after` y `fnonce-namespace-tests`.

El candidato se inserta en `completeNamespaceFunctionCall`, después de que el compilador elige
el ordinal semántico de función y sus ordinales de parámetros. Usa el adaptador del candidato
seleccionado y los bindings de locales de los argumentos; no busca el destino por nombre.
Reutiliza `directCallableParameterInvocations` para el subconjunto lineal ya aplicado a métodos.
Excluye receptores de extensión, funciones externas o precompiladas y definiciones ausentes.
No cubre forwarding, ramas, bucles en el callee ni llamadas de delegado de origen desconocido.

El parche `A:\nsc-fixes-agent1-temp\fnonce-namespace-prototype.patch` modifica solamente
`src/Frontend/Bodies/CallAnalyzer.cpp` sobre la copia WIP congelada SHA-256
`e07e5a8d4aaf13dba07474c6782b5b85e7b29e93f344576e97c39cf57e1328d5`. La prueba portable
`tests/integration/fnonce_namespace_parameter.py` incluye dobles llamadas por función y local,
argumentos nombrados, aliases, sobrecargas, nombres iguales en namespaces distintos, `Fn` y
llamadas originadas dentro de un namespace. Los dos últimos casos de caller en namespace se
añadieron después de suspender las pruebas y están completamente sin ejecutar.

La compilación incremental integrada pasó. En la repetición dirigida, las seis
pruebas FnOnce y dos de préstamos pasaron, sin omisiones; los logs son
`A:/nsc-fixes-evidencia/namespace-fnonce-integrated-20261111.log` y
`A:/nsc-fixes-evidencia/namespace-lambda-borrow-20261111.log`. Se incluyen los
casos de caller en namespace. El compilador contenía otros cambios locales y
la prueba no certifica una revisión limpia ni las rutas no cubiertas citadas.
