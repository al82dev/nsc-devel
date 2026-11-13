# Llamadas lanzadoras con finally suspendible

En `24aaaf29d`, los casos originales `async_cleanup_suspends` y
`generator_cleanup_suspends` terminaban con excepción no capturada en O0/O2.
El puente tipado rechazaba el `finally` por contener un `await`; el fallback
ejecutaba su limpieza durante el unwind nativo.

El puente admite un solo `finally` suspendible cuyo cuerpo pertenece al
subconjunto comprobado, incluidos `await` de un future sin error y lecturas
locales int. Conserva la excepción en un slot del frame, disponible al
reanudar, y la publica después de completar los cleanups. Las excepciones que
reemplazan a la anterior mantienen el mecanismo tipado existente. Las formas
que no satisfacen estas condiciones conservan el fallback anterior.

La regresión `async_call_suspending_cleanup.py` ejecutó 14 casos O0/O2,
incluidos los dos originales, reemplazo posterior al `await`, dos suspensiones,
limpieza anidada y controles de captura. Pasaron los nueve casos de
`async_cancel_drop_cleanup.py` en ambos niveles. Evidencia:
`A:\nsc-fixes-evidencia\async-call-suspending-regression-extended-20261112.log`
y `A:\nsc-fixes-evidencia\async-call-suspend-drop-controls-20261112.log`.

La prueba `async_cancel_finally_continuation.py` sigue fallando en los dos
casos de cancelación con `yield` dentro del finally; los mismos resultados
constan previamente en `async-yield-scalar-regression-20261112.log`. No se ha
demostrado cancelación mientras está pendiente la excepción de este puente,
ni el tratamiento de un future hijo con error dentro de este finally. El cambio
no acredita esos caminos ni excepciones externas.

Después de integrar las tres correcciones de error y cleanup, la prueba
original completa `async_poll_call_exception.py` pasó sus 18 casos en O0 y O2,
36 ejecuciones, con el compilador aislado de `9a219326e`. El comando y cada
resultado están en
`A:\nsc-fixes-evidencia\async-poll-call-exception-current-20261112.log`.
Esta prueba dirigida no verifica las limitaciones de cancelación indicadas.
