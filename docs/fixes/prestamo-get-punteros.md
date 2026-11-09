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
el alias conserve la prohibición de escritura.

## Propagación del préstamo de solo lectura

El análisis semántico conserva la restricción en variables prestadas, campos
de referencia anidados, conversiones de referencia, expresiones condicionales
y `??`. Rechaza una reasignación que intentaría introducir el préstamo de solo
lectura en una variable prestada mutable, para evitar perder la restricción en
una unión de flujo o en una vuelta de un ciclo. También impide retornar ese
objeto mediante un contrato prestado mutable.

Como los métodos de clase, getters y parámetros actuales no acreditan un
contrato de receptor u objeto inmutable, las llamadas correspondientes, la
conversión a delegado, el pase a argumentos y los constructores se rechazan
conservadoramente en los casos comprobados. Esta restricción puede impedir
operaciones que serían seguras; no demuestra que exista una API general de
parámetros de clase de solo lectura. El acceso a campos escalares se mantiene.

En la copia y build aislados de A, con el compilador autorizado y los
temporales en A, las comprobaciones dirigidas registraron:

- `shared-control-suite.py`: 5/5, con los controles internos local y atómico,
  lecturas y accesos vacíos en O0/O2 y rechazos de escritura por alias,
  cadenas, campos anidados, conversiones, condicionales, métodos, propiedades,
  indexadores, delegados, parámetros, constructores y retorno mutable.
- `nested-thread-safety-tests.py`: 2/2; `run-reference-unit-tests.py`: 2/2.
- `clang-tidy --checks=clang-analyzer-*` en los diez archivos C++ modificados y
  una segunda pasada en los archivos modificados después del primer análisis:
  salida 0. Los 11 registros de la primera serie contienen 55 avisos de padding
  de encabezados, sin otros tipos de warning observados en esa serie. Las
  salidas están en `A:/nsc-fixes-evidencia/readonly-borrow-analysis-*.log` y
  `readonly-borrow-final-*.log`; el resumen cuenta avisos repetidos por unidad.

La evidencia no cubre todas las formas posibles de conversión, captura,
metadata precompilada ni todas las combinaciones de flujo. La garantía de
`Send`/`Sync`, la vida del módulo productor y las cuatro API públicas siguen
pendientes. No se ejecutó una batería completa, por instrucción del usuario.
