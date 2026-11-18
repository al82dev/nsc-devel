# Consulta de StrongCount con vista activa

La protección de llamadas sobre dueños con vistas activas rechazó el control
existente `safe` al consultar `owner.StrongCount()`. La salida anterior está en
`A:/nsc-fixes-evidencia/shared-argument-controls.log`. El mismo control ejecutó
42 en O0/O2 con el compilador anterior a esa protección y las fuentes actuales
(`shared-safe-before-receiver.log`).

Los métodos StrongCount de Rc y Arc y sus ayudas internas solo observan el
estado de contadores; ahora declaran readonly su receptor para que el compilador
distinga esa consulta de un método que puede reemplazar el dueño. No se cambia
el contrato de conteo ni se afirma que la observación concurrente sea estable.

El control `safe` y cuatro controles dirigidos de préstamos pasaron para Rc y
Arc (`shared-observer-controls.log`). `shared_count_during_view.py` compiló y
ejecutó con SDK binario en O0/O2, con salida 42 (`shared-observer-only.log`).
La prueba binaria se invoca directamente porque CMake contiene cambios ajenos.
El SDK se pidió con caché incremental, pero hubo miss y se compilaron sus 71
fuentes (`shared-observer-sdk.log`). Ese build incluyó fuentes concurrentes sin registrar y las pruebas
usaron el compilador con una corrección de argumentos todavía en curso; no
certifican el commit aislado ni seguridad de concurrencia completa.

## Consulta de WeakCount

Con una vista de `owner.Borrow()` activa, `owner.WeakCount()` era rechazado
como llamada potencialmente mutadora en Rc y Arc. Las reproducciones binarias
anteriores están en `A:/nsc-fixes-evidencia/shared-weakcount-view-before.log`
y `shared-weakcount-view-arc-before.log`. Las implementaciones delegadas de
WeakCount leen los contadores y no reemplazan el dueño. Se declararon `readonly`
los receptores en Rc, Arc, SharedLocalHandle y SharedAtomicHandle; no se cambió
la aritmética del conteo. La observación de Arc sigue siendo aproximada durante
operaciones concurrentes y exacta si no las hay.

La regresión `tests/integration/shared_weakcount_during_view.py` mantiene un
débil y una vista activos, consulta ambos contadores y lee el payload. Pasó
para Rc y Arc en O0/O2 desde fuentes y mediante SDK binario aislado:
`shared-weakcount-view-source.log` y `shared-weakcount-view-binary.log`. También
pasaron los controles dirigidos de lectura, rechazo de escritura y préstamos
`shared-weakcount-controls.log`.

El SDK binario se construyó en `shared-weakcount-sdk` usando la caché
incremental existente; hubo `compilation-bundle-cache: not-found` y el
compilador procesó 71 fuentes, entre ellas `DeterministicMath.ns` con cambios
concurrentes sin registrar (`shared-weakcount-sdk.log`). Por tanto, la prueba
binaria demuestra el comportamiento de ese artefacto, no un SDK de árbol limpio.
No se ejecutó batería completa ni se certifica seguridad concurrente global.
