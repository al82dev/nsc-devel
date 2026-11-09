# Acceso prestado a los controles compartidos

El usuario eligió que `Rc.Get()` y `Arc.Get()` devuelvan una referencia
prestada de solo lectura ligada al propietario, tanto para clases como para
estructuras. El acceso directo se añadió primero a los controles **internos**
local y atómico; las cuatro familias públicas todavía no están implementadas.

El prototipo mostró que `Payload escaped = owner.Get()` podía copiar una
referencia de clase y usarla después de destruir el propietario. El análisis
de llamadas trata ahora como prestado el resultado de clase obtenido mediante
un retorno de referencia de instancia que no declara vida estática. Para un
préstamo local tomado de una referencia que depende de `this`, el compilador
registra como origen al receptor local con movimiento exclusivo. Las llamadas
con vida estática explícita mantienen su contrato y las que declaran otra
vida no se vinculan arbitrariamente al receptor.

Las pruebas dirigidas `nsc.sdk.shared-internal-handles` comprueban para los
controles local y atómico el acceso a clases y estructuras en O0/O2, la
lectura mediante alias prestado, el rechazo de escritura en el valor de una
estructura, la prohibición de mover al propietario mientras vive el préstamo
y el rechazo de escapes por local, campo, retorno y argumento. También pasan
las otras cuatro pruebas de controles y almacenamiento compartido, la
comprobación semántica de Core y las pruebas dirigidas de vistas y seguridad de
hilos (`shared-control-suite.log`, `shared-core-check.log` y
`nested-thread-safety-tests.log` en `A:/nsc-fixes-evidencia`).

Esta evidencia no demuestra aún la seguridad de todas las formas de receptor,
las referencias cruzando módulos precompilados ni la transferencia segura
de `Arc` entre hilos. La API pública sigue pendiente hasta acreditar el pin
del módulo productor y el contrato `Send`/`Sync` apropiado. `WeakCount()`
atómico tendrá el contrato elegido: aproximado ante concurrencia y exacto
cuando no hay operaciones simultáneas.

Una comprobación posterior encontró que escribir `owner.Get().Data` es
rechazado, pero `borrowed Payload alias = owner.Get(); alias.Data = 100l;`
es aceptado (`A:/nsc-fixes-evidencia/borrowed-class-mutation-probe.log`).
Por separado, una clase con campo público mutable satisface actualmente una
restricción `Sync` (`probe-sync-mutable-class/results.json`). Estas dos
observaciones no son una prueba de seguridad de `Arc`. El usuario eligió que
el alias conserve la prohibición de escritura. Esta decisión está pendiente
de implementar y validar, incluidos métodos mutadores, pases por argumentos y
propagación a otros alias; no basta prohibir la escritura directa de campos.
