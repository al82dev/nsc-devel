# Campo indexado por un dueño genérico

En `7fa97ff72`, un `GenericLookup<1>` con getter que devuelve `Holder<N>`
rechaza `items[0].Value.At<20, 42>()` en O0/O2. El control que guarda primero
el resultado como `Holder<1>` ejecuta 42. Fuentes, comandos y diagnósticos:
`A:/nsc-fixes-agente-2-evidencia/generic-indexer-field-before/results.json`.
El mismo ejemplo con el cambio ejecuta 42 en ambos modos:
`generic-indexer-field-after/results.json` bajo la misma carpeta. El productor
original conservó SHA-256 `f2e792e179c821b04dc5eb6d4c34835e0be17eb74e5ed4587c9e17c328b2df09`
y el consumidor directo `33cb17d96d3c0d42b86bf52cae6361bd67b5dec9a681a1a23b7b536b7540ff0f`.

`ProjectGenericModel::processGenericCall` ya identificaba el getter único de
un dueño ordinario. La misma ruta reconoce ahora un dueño genérico único y
cerrado y sustituye los argumentos en el retorno del getter con el mecanismo
de especialización existente. La resolución del retorno utiliza el ámbito de
su declaración. Se conserva la restricción a un getter público único, sin
bases; los demás casos siguen la ruta anterior.

La prueba `generic_owner_indexer.py` valida dos especializaciones con valores
distintos, un paquete de tipos, un índice de pack inválido y el rechazo de un
campo privado. Los controles anteriores de identidades homónimas, fuentes y
productores/consumidores estáticos y DLL se ejecutaron en O0/O2. También
pasaron diez regresiones dirigidas. Registros en
`A:/nsc-fixes-agente-2-evidencia/generic-generic-indexer-field-fix/`.

El compilador experimental combina objetos de esta rama con bibliotecas
copiadas del build de campaña; comandos y hashes están en `build.log`. Falta
validar con el compilador integrado. No se afirma cobertura de getters
sobrecargados, herencia ni todas las formas de argumentos genéricos.
Verificación integrada, 2026-11-11: la compilación incremental y
`nsc.generics.owner-method-indexer` pasaron, 1/1 sin omisiones. Los comandos y
resultados están en `A:\nsc-fixes-evidencia\generic-owner-generic-indexer-field-20261111.cmd`
y su log homónimo. El compilador contenía otros cambios locales y no certifica
una revisión limpia; los límites de selección y herencia indicados abajo siguen
vigentes.
