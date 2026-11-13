# Excepciones de llamadas con limpieza async sin suspensión

En `81726c1f1`, las reproducciones `async_cleanup`,
`async_cleanup_replacement` y `generator_cleanup` de
`tests/integration/async_poll_call_exception.py` terminaban con excepción no
capturada en O0 y O2. El MIR de `finally` contiene un cuerpo seguido de
`CleanupContinuation`; `supportsAsyncCallCleanup` solo aceptaba un hijo e
impedía utilizar el puente tipado existente. El caso de reemplazo contiene
además una declaración local `int` sin región de cleanup ni propiedad.

El puente admite ahora esa forma de `finally` y esa declaración escalar, sin
eliminar las restricciones sobre los demás nodos. La prueba dirigida
`tests/integration/async_poll_call_cleanup_nonsuspending.py` reutiliza los
casos originales y comprueba las excepciones, su destrucción, el reemplazo,
los cleanups anidados y su orden, junto con controles de llamadas y captura.
Sus 14 casos pasaron en O0 y O2 usando el compilador incremental aislado;
la evidencia está en `A:\nsc-fixes-evidencia\async-poll-cleanup-controls-20261112.log`.

Los casos `async_cleanup_suspends` y `generator_cleanup_suspends` siguen
fallando en O0 y O2; no están cubiertos por esta corrección. La ejecución
completa de la prueba original no se declara aprobada. Tampoco se acredita con
esta prueba la interoperabilidad C/C++ ni el comportamiento de excepciones
externas.
