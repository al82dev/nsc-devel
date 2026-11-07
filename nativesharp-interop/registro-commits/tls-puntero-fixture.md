# Reproducción de puntero global TLS C

El control C del fixture estático pasó: asignó y desreferenció `int*` TLS y
comprobó aislamiento de otro hilo. NSC `8e8795a3d555ae35` rechazó en O0 cada
uso del puntero con `thread-local C/C++ global is not yet projected`. La
variante DLL con CRT estático está preparada y aún no se ha ejecutado porque
el caso estático se detuvo antes. Se mantienen ambos en la tanda 8.
