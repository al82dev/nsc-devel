# Restricciones Send y Sync con tuplas

Una estructura que contiene `(Raw, int)`, donde `Raw` contiene `int*`, se
aceptaba como `Send` y como `Sync`. También se aceptaba como `Sync` una tupla
que contiene una estructura con `ref mut int`. La matriz anterior, incluyendo
una tupla anidada y un control seguro, está en
`A:/nsc-fixes-evidencia/send-tuple-probe/before.json`; la regresión falló al
aceptar `Send:WrappedRaw` antes del build (`regression-before.log`). No se
ejecutó código concurrente.

El recorrido de `satisfiesThreadSafetyConstraint` seguía campos nominales y
elementos de arreglos, pero no los elementos estructurales de una tupla. Ahora
los incorpora al recorrido, junto con arreglos anidados y referencias mutables.
Una referencia compartida encontrada dentro de esa estructura se rechaza de
forma conservadora: este recorrido todavía no transporta por separado la
exigencia de `Sync` del referente cuando se comprueba `Send`. Puede rechazar
un tipo seguro; no se declara resuelto ese caso.

La compilación incremental se realizó en A. La matriz dirigida en
`tests/integration/send_mutable_reference.py` pasó con Core desde fuentes
(`send-mut-ref-check.log`) y el SDK binario aislado (`send-mut-ref-binary-check.log`):
rechaza `WrappedRaw` y su forma anidada para ambas garantías; acepta el control
seguro para ambas y acepta el valor con referencia mutable solo para Send.
También pasaron `nested-thread-safety-tests.log` y
`generic-constraints-existing.log`. Los logs están en `A:/nsc-fixes-evidencia`.
La prueba se ejecuta directamente mientras el archivo CMake compartido tiene
cambios ajenos, que también coexistieron con el build.

Esto no demuestra todas las formas de referencia ni seguridad concurrente de
las familias públicas; `Arc` y `WeakArc` siguen sin habilitarse entre hilos.
