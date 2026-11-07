# Matriz de selección conservadora por TLS

El fixture prepara una biblioteca C con TLS interno cuyo header no revela el
global. Comprueba selección LLD y ejecución O0–O3 sin opción ni variable,
un consumidor de una biblioteca NativeSharp precompilada con ese TLS, un
proyecto sin TLS que conserva NSLink y la precedencia explícita de NSLink en
línea de comandos y sidecar. Usa únicamente rutas de trabajo en A: y no
modifica NSLink ni fixtures anteriores.

Con NSC build `2c3c6e700529d377`, la matriz pasó TLS interno O0–O3, TLS en
biblioteca NativeSharp precompilada O0, proyecto sin referencias C, biblioteca
C sin TLS y precedencia de NSLink por opción, entorno y sidecar. La selección
explícita LLD, el consumidor DLL O0–O3 y los controles de caché también pasaron.
Una detección positiva de `_tls_index` es conservadora y una negativa no
demuestra ausencia de TLS; se mantienen abiertas las demás variantes y la
validación completa de la tanda 8.
