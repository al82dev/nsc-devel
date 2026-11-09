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
