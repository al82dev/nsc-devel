# Reproducciones dirigidas de async

En `987f3b52c`, la repetición dirigida de
`async_nested_cancel_cleanup.py` mantuvo el fallo `nested_cancel_first` con
salida 22 en O0/O2; el control `nested_normal` devolvió 42 en ambos niveles.
Evidencia: `A:/nsc-fixes-evidencia/nested-cancel-after-987f3b52c.log`. El
trabajo anterior modificó componentes numéricos y de vistas, no la ruta de
cancelación; esta comprobación confirma que el caso sigue abierto.

El 12 de noviembre, en `development-fixes-limitaciones`, se usó el compilador
aislado `A:/nsc-fixes-evidencia/build/bin/nsc.exe`, build hash
`b2a62146822e8e54`, SHA-256
`e6b7ea532d92443d1f0e5900b9de1a426e5c5fbb81b367416e789aa824d157f4`.
El build existente apunta a `A:/nsc-devel-fixes`; no se reconstruyó para estas
repeticiones y su procedencia exacta respecto al HEAD no queda certificada.
Los comandos fijan temporales y runtime en A; `nslink.exe` se resuelve por PATH.
No se modificaron los ejemplos originales.

## Cancelación

El runner `tests/integration/async_example88_deferred_cancel.py` usa la raíz
del repositorio como directorio de ejecución de sus procesos. Las ocho
combinaciones dirigidas O0/O2 devolvieron 42; el comando y la salida son
`A:/nsc-fixes-evidencia/example88-cwd-recheck-20261112.cmd` y `.log`. Esto
acredita los casos de ese runner, no los finally anidados ni todos los
valores movidos o prestados.

La reproducción adicional `A:/nsc-fixes-evidencia/recheck-cancel-nested-20261112.log`
registra que `nested_cancel_first` devuelve 22 en O0/O2; una variante de traza
confirma que se publica 6 después de `Cancel()`. `cancel_plain_await_side_effect`
también devuelve 22, y `nested_cancel_await_side_effect` no compila por
dominancia de `cleanuppad` en ambas optimizaciones. Estas rutas siguen abiertas.
El contrato exige limpieza en el siguiente avance sin publicar otro elemento;
el prototipo previo limitado a escalares no demuestra propiedad ni suspensión.

La regresión `tests/integration/async_nested_cancel_cleanup.py`, registrada
como `nsc.compiler.async-nested-cancel-cleanup`, conserva el caso anidado y un
control de avance normal. En la ejecución directa documentada por
`A:/nsc-fixes-evidencia/nested-cancel-regression-20261112.cmd` y `.log`, el
caso cancelado devuelve 22 en O0/O2 y el control normal devuelve 42 en O0/O2;
el runner termina con fallo, como corresponde mientras no exista corrección.
El registro CMake no se ejecutó en esta comprobación.
La repetición con el runner portable y TEMP efectivo en A conserva el mismo
resultado; `A:/nsc-fixes-evidencia/async-portable-temp-verify-20261112.log`
registra también cwd y hash del compilador antes y después. La marca de
verificación del lanzador indica que observó el fallo esperado, no que la
regresión haya pasado.

## Sustitución de excepciones

El runner común `tests/integration/async_catch_owner_continuation.py` ejecuta
sus procesos desde la raíz del repositorio y ya no impone una versión de
lenguaje al proyecto temporal. Con ese runner, el comando y resultado en
`A:/nsc-fixes-evidencia/async-finally-20261111/replacement-no-version-20261112.cmd`
y `.log` muestran cuatro controles síncronos con salida 42 en O0/O2.
`sync_inner_catch` sigue rechazado por acciones excepcionales dentro de un
cleanup SEH; `async_ready_replace` y `async_suspended_replace` por una salida
estructurada desde ese cleanup, en ambos niveles. El fallback para excepciones
ajenas debe preservarse; omitirlo sería incompleto. Ninguno de estos fallos se
presenta como corregido.
