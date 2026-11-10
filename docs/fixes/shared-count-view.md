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
