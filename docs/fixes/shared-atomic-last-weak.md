# Último débil explícito durante la destrucción

La prueba dirigida `tests/integration/shared_atomic_last_weak.py` comprueba el
control atómico interno cuando el destructor del último fuerte permanece en
ejecución y otro hilo libera el último débil explícito. Se coordina la entrada
al destructor antes de liberar el débil, y se permite terminar al destructor
solo después de comprobar que esa liberación devuelve cero: el débil implícito
todavía conserva el bloque. Tras unir el hilo se comprueba que la liberación
del fuerte devolvió uno y que el destructor se ejecutó una vez por ronda.

El compilador aislado `A:/nsc-fixes-evidencia/build/bin/nsc.exe` ejecutó 64
rondas en O0 y 64 en O2; ambas ejecuciones devolvieron 42. El comando completo,
las fuentes y los resultados están en
`A:/nsc-fixes-evidencia/shared-atomic-last-weak.log`. Los temporales y el
runtime usados pertenecen al entorno aislado de A.
La configuración se actualizó mediante build incremental y la entrada
`nsc.sdk.shared-atomic-last-weak` pasó 1/1 en CTest; el registro está en
`A:/nsc-fixes-evidencia/shared-atomic-last-weak-ctest.log`.

Esta prueba no observa directamente la liberación final del bloque, no cubre
todas las intercalaciones y no demuestra que las familias públicas `Arc` y
`WeakArc` puedan transferirse entre hilos. Tampoco acredita la retención del
módulo productor. No se cambió la implementación del control atómico.

## API pública y Core binario aislado

La prueba dirigida `tests/integration/shared_arc_public_last_weak.py` ejercita
ahora las familias públicas con el Core binario preexistente de
`A:/nsc-fixes-evidencia/pointers-sdk-refresh`. En cada una de 64 rondas, el
destructor del último `Arc` anuncia su entrada y espera; otro hilo libera el
último `WeakArc` explícito antes de permitirle terminar. La ejecución verifica
una destrucción por ronda y el estado vacío del débil después de `Join`.
Las ejecuciones sin ASan devolvieron 42 en O0/O2; con la opción ASan activada
también devolvieron 42 sin diagnóstico, usando `ASAN_OPTIONS=detect_leaks=0`.
Comandos y salidas: `arc-public-last-weak.cmd`, `arc-public-last-weak.log`,
`arc-public-last-weak-asan.cmd` y `arc-public-last-weak-asan.log` en la carpeta
de evidencia. El primer fixture falló porque el constructor implícito de la
carga no era accesible desde la plantilla binaria; se declaró explícitamente
un constructor público y ese primer resultado se conservó aparte.

El compilador usado tiene SHA-256
`5e16826475652767add778eaa966c24da404cb769ec13a9ad0fb745116afb74d` y el Core
preexistente `f91d95c248f477531d38a093605f45343f6be9736d15e2097acd452db4bb54e0`.
La barrera acredita ese orden concreto, no todas las carreras. El Core binario
no fue instrumentado por ASan y el módulo permanece cargado; por ello no se
certifican todos los accesos internos ni la descarga del módulo. No se
regeneró Core ni se modificó la implementación de los contadores.

La prueba pública se registró en CTest y se repitió bajo ASan con el compilador
incremental de integración y el Core binario `pointers-readonly-sdk`; pasó 1/1
sin omisión y devolvió 42 tanto en O0 como en O2. Comando y salida:
`A:/nsc-fixes-evidencia/arc-last-weak-asan-ctest-20261111.cmd` y `.log`.
Sin `NSC_TEST_CORE_BINARY`, la prueba registrada se marca como omitida; el
resultado citado sí lo configuró explícitamente. Se usó `detect_leaks=0` y el
Core precompilado no estaba instrumentado. Esto no demuestra ausencia general
de fugas, todas las intercalaciones ni descarga dinámica segura.
