# Reproducciones dirigidas de async

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
