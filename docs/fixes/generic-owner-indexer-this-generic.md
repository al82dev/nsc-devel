# `this[index]` en un dueño genérico cerrado

## Reproducción y corrección

En `4c131f94c`, una llamada a `this[0].At<20, 42>()` dentro de
`GenericLookup<N>` falla al cerrar `GenericLookup<1>`, aunque el acceso al
indexador desde otra variable del mismo tipo funciona. Registro anterior:
`A:/nsc-fixes-agente-2-evidencia/generic-indexer-this-generic-before.log`.

El dueño especializado se encuentra en `specializations_`, pero no en el
índice de declaraciones ordinarias. Durante el recorrido observado, el dueño
se representa como el nombre de la especialización repetido con `+`; la traza
está en `generic-indexer-this-generic-trace/generic_owner_indexer.log`. En la
ruta de indexador se admite ahora el clon ya registrado cuando su nombre
completo, namespace, proyecto y plantilla de nivel superior lo identifican de
forma única. Se usa su sintaxis ya sustituida y el contexto de la plantilla;
no se deducen argumentos a partir del nombre. Si no hay identidad única se
conserva la ruta anterior. Los dueños genéricos anidados no se consideran
resueltos por este cambio.

## Pruebas y límites

`generic_owner_indexer.py` comprueba dos especializaciones distintas, acceso
desde `this` y desde una variable, controles de ambigüedad y accesibilidad, y
consumo de bibliotecas estáticas/DLL con productor y consumidor O0/O2. Incluye
un productor genérico mínimo y su consumidor binario. También pasaron diez
regresiones dirigidas. Registros en
`A:/nsc-fixes-agente-2-evidencia/generic-indexer-this-generic-fix2/`.

Un productor mínimo con el método sobre `this` se publica y su consumidor
devuelve 42 en O0/O2 (`generic-indexer-this-generic-producer/results.log`).
El fixture más amplio con ese mismo método aún falla al publicar metadata:
`reusable template closure is incomplete`, con `At<...>` como plantilla
faltante. Los comandos y ejemplos de ese límite independiente están en
`generic-indexer-this-generic-producer-wide/results.log`; no se ha aislado la
declaración adicional que lo causa.

El ejecutable es híbrido: tres objetos de esta rama y bibliotecas copiadas del
build de campaña. `build.log` conserva comandos y hashes. Falta validar con
el compilador integrado; esta evidencia no demuestra un build limpio.
Verificación integrada, 2026-11-11: la compilación incremental y la prueba
`nsc.generics.owner-method-indexer` pasaron; CTest ejecutó 1/1 sin omisiones.
Comando y resultados en
`A:\nsc-fixes-evidencia\generic-indexer-this-generic-20261111.cmd` y `.log`.
El compilador contenía otros cambios locales y no certifica un árbol limpio;
el productor amplio descrito abajo permanece pendiente.
