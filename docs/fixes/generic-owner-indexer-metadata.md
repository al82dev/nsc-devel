# Dependencia de método genérico sobre `this[index]`

## Reproducción

En `819a1312b`, la biblioteca que declara `GenericLookup<N>.ReadThis` con
`this[0].At<...>()` se publica si `At` es único. Agregar un método genérico
`At` en otro tipo, con nombre de tipo igual o distinto, hace que la clausura
de metadata rechace el productor; renombrar el método adicional permite
publicarlo. Los controles y comandos originales están en
`A:/nsc-fixes-agente-2-evidencia/generic-indexer-closure-isolation/`. El
productor amplio original, que también falla en O0/O2, está en
`generic-indexer-this-generic-producer-wide/` de la misma carpeta de evidencia.

Un control adicional revela que, si el tipo retornado no tiene `At` pero otro
tipo sí, el colector anterior acepta esa dependencia equivocada. Ese resultado
previo está en `generic-indexer-closure-isolation-after/minimal-missing-target/`.

## Causa y cambio

`ProjectGenericModel::collectCallDependency` no identificaba el resultado del
indexador y buscaba el método por nombre sin dueño. Ahora, para `this[index]`
en la plantilla del dueño, si hay un único indexador propio, getter de la
aridad escrita y tipo genérico de retorno resuelto sin bases, registra los
métodos aplicables cuya declaración pertenece a ese tipo. Si el tipo resuelto
no declara el método, conserva el error de dependencia faltante en lugar de
elegir uno homónimo. Cuando no se cumplen las condiciones conserva la ruta
anterior; no cambia el formato de metadata ni las reglas semánticas.

## Validación y límites

Los controles mínimo, amplio, homónimos y método distinto publican en O0; el
control sin método propio se rechaza. Comandos y resultados están en
`generic-indexer-closure-isolation-checked/`. El control con imports retirados
falla por nombres que ya no existen y no es un ejemplo válido del defecto.

`generic_owner_indexer.py` comprueba la publicación y consumo del productor
amplio en estática y DLL, productor y consumidor O0/O2, además del productor
mínimo, llamada sobre `this`, dos especializaciones y el rechazo de la falsa
dependencia. Pasaron diez regresiones dirigidas adicionales. Logs en
`generic-indexer-closure-checked/` bajo la carpeta de evidencia indicada.

El ejecutable experimental combina tres objetos de esta rama con bibliotecas
copiadas del build de campaña; `build.log` conserva comandos y hashes. Falta
repetir las pruebas con el compilador integrado. No se afirma soporte general
para indexadores sobrecargados o tipos de retorno con herencia.
Verificación integrada, 2026-11-11: la compilación incremental y
`nsc.generics.owner-method-indexer` pasaron, con CTest 1/1 sin omisión. El
log `A:\nsc-fixes-evidencia\generic-indexer-metadata-20261111.log` conserva
los rechazos esperados de productores con dependencias faltantes, además de
los casos válidos. El compilador incluía otros cambios locales; no constituye
una validación de revisión limpia. Las sobrecargas y herencia siguen fuera de
la cobertura de esta corrección.
