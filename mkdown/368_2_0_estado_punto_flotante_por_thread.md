# 368 — Estado floating-point por thread / Per-thread floating-point state

## Español

Las políticas de modo, contracción FMA, redondeo y subnormales ahora forman un único
`FloatingPointState` local a cada hilo. Una compilación concurrente ya no puede cambiar el
contrato de otra. `captureFloatingPointState()` permite transportar la configuración y
`FloatingPointModeScope` restaura el estado completo al salir.

La prueba `nsc.item368.thread-fp-state` sincroniza dos hilos con políticas opuestas y verifica el
aislamiento, los valores iniciales estrictos y la restauración del scope.

## English

Mode, FMA contraction, rounding, and denormal policies now form one thread-local
`FloatingPointState`. Concurrent compilations can no longer overwrite each other's contract.
`captureFloatingPointState()` transports the configuration and `FloatingPointModeScope` restores
the entire previous state on exit.

`nsc.item368.thread-fp-state` synchronizes two threads with opposing policies and verifies
isolation, strict initial values, and scope restoration.
