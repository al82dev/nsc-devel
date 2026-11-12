# Excepciones de llamadas en un poll async

## Evidencia y estado

En el compilador de referencia SHA-256
`90a64254d0627b599b2f52edb495ee840ad4e3c9e58252f783b617ff01aab479`,
una llamada que lanza tras una suspensión produce `0xe04e5301` en función y
generador async, en O0 y O2. La llamada síncrona equivalente, throw directo y
catch local devuelven 42. El runner dirigido, comandos y salida están en
`A:/nsc-fixes-evidencia/async-worker2-poll-baseline.log`.

Se integró como trabajo en curso un cambio acotado para un poll sin handler local, funclet, cleanup,
catch activo ni celdas capturadas, cuyo error se almacena como referencia de
clase. El filtro SEH tipado existente acepta sólo objetos compatibles con el
tipo de error declarado; si no se puede resolver ese filtro, se conserva el
unwind anterior. Tras `catchret`, se
transfiere el objeto al future, se limpia TLS y se publica estado -2. El
catchswitch no consume excepciones ajenas y deja su unwind al caller. El
generador además marca terminado el iterador. Es un cambio **no compilado ni
ejecutado** en esta revisión y no constituye una solución general.

## Pruebas preparadas y límites

`tests/integration/async_poll_call_exception.py` verifica identidad y cantidad
de excepciones, llamadas, catch local, cleanups y control normal en O0/O2.
Los casos añadidos después de la línea base —reemplazo, destructor que lanza,
finally suspendible y orden anidado— aún no se han ejecutado. Los cleanups
activos quedan fuera del cambio acotado y permanecen pendientes; tampoco se ha
validado la barrera de tipo con un objeto incompatible ni el manejo de celdas.

`tests/integration/async_poll_foreign_exception.py` prepara un control híbrido:
genera IR con NSC, sustituye sólo el cuerpo de `Fail` por una excepción ajena
y agrega un observador SEH; exige código original, TLS vacío y los cleanups
esperados. Esos controles no se han ejecutado con el cambio. El control híbrido
anterior con cleanup pasó O0/O2 en el compilador de referencia, con advertencia
de triple de destino, y se conserva en `A:/nsc-fixes-evidencia/
async-worker2-foreign-check.log`.

La solución general necesita preservar la excepción pendiente y el cursor de
continuación a través de un finally suspendible; publicar -2 antes de terminar
puede hacer visible el error prematuramente. También requiere orden léxico,
destructores que reemplazan la excepción, celdas capturadas y fallback foreign.
El diseño y sus riesgos se conservan en
`A:/nsc-fixes-evidencia/async-worker2-call-eh-design.md`.

Una extracción mecánica adicional del helper de continuación de await está en
el clon como preparación independiente; no forma parte de este cambio acotado,
no ha sido probada y no debe integrarse sin la solución general.
