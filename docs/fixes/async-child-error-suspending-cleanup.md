# Error de future hijo con limpieza suspendible

En `c085093df`, `generator_child_error_cleanup_suspends` devolvía 3 en O0 y
O2: la excepción llegaba al consumidor antes de que el `finally` terminara.
`emitSuspendingAwaitErrorPath` publicaba el estado de error y terminaba el
generador antes de ejecutar los cleanups, aunque ya publicaba el error de
nuevo después de ellos.

Se difieren ambas operaciones terminales hasta completar la limpieza. La
regresión `async_child_error_suspending_cleanup.py` pasó en O0 y O2 para el
ejemplo original de generador, un async ordinario y controles de captura local
en ambos tipos. Evidencia:
`A:\nsc-fixes-evidencia\async-child-cleanup-regression-20261112.log`.
El control de throw directo con cleanup suspendible también pasó en la
reproducción posterior. Las llamadas que lanzan mediante unwind con cleanup
suspendible siguen fallando y se mantienen pendientes.
