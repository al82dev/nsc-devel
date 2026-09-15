# 376 — Estado floating-point de workers / Worker floating-point initialization

## Español

`CompilationWorkerPool` acepta ahora un inicializador ejecutado una vez al comienzo de cada worker.
La generación OOP paralela captura el `FloatingPointState` del hilo que inicia la compilación y lo
instala en los workers usados para lowering, optimización y emisión de objetos. Así, modo preciso,
rápido o determinista, política FMA, redondeo y tratamiento de subnormales no vuelven silenciosamente
a los valores predeterminados al cruzar un límite de thread.

`initializeFloatingPointStateForWorker` instala el contrato en un thread recién creado y de vida
acotada. A diferencia de `FloatingPointModeScope`, no restaura un estado anterior: el worker posee
esa configuración hasta terminar. El hilo que envía el trabajo conserva intacto su propio estado,
manteniendo el aislamiento introducido por el ítem 368.

La prueba inicia tres workers con una combinación no predeterminada, los mantiene activos al mismo
tiempo y comprueba que cada índice observa exactamente las cuatro políticas capturadas. También
valida que el hilo emisor no cambie y vuelve a ejecutar las pruebas del estado por thread, del pool
de compilación y del lowering OOP.

## English

`CompilationWorkerPool` now accepts an initializer that runs once at the beginning of every worker.
Parallel OOP generation captures the submitting thread's `FloatingPointState` and installs it in
workers used for lowering, optimization, and object emission. Precise, fast, or deterministic mode,
FMA policy, rounding, and denormal handling therefore no longer silently revert to defaults across a
thread boundary.

`initializeFloatingPointStateForWorker` installs the contract in a newly-created, bounded-lifetime
thread. Unlike `FloatingPointModeScope`, it does not restore a prior state: the worker owns that
configuration until it exits. The submitting thread retains its own state unchanged, preserving the
isolation introduced by item 368.

The test starts three workers with a non-default combination, keeps them active concurrently, and
checks that every worker index observes all four captured policies exactly. It also confirms that the
submitting thread remains unchanged and reruns per-thread state, compilation-pool, and OOP-lowering
tests.
