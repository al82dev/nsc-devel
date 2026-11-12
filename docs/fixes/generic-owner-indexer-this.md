# Llamada genérica sobre `this[index]`

## Reproducción

En `07319020e`, dentro de `PrivateCandidate`, el control guarda
`this[index]` en un local y llama al método genérico; también funciona llamar
sobre `self[index]` cuando `self` es un local de tipo `PrivateCandidate`.
En cambio, `this[index].At<0, 42>()` se rechaza porque no se encuentra la
plantilla del método. El log anterior a la corrección está en
`A:/nsc-fixes-agente-2-evidencia/generic-indexer-private-final/generic_owner_indexer.log`.
Los controles de la variante con `self` pasaron previamente en fuente y en
bibliotecas; el log de esa ejecución anterior se conserva en
`A:/nsc-fixes-agente-2-evidencia/generic-indexer-private-fix/`.

## Causa y corrección

En `ProjectGenericModel::processGenericCall`, la ruta que infiere el dueño de
un método genérico cuyo receptor es un indexador pide el tipo de la colección.
La inferencia general no da uno para `ThisExpression`. En esa ruta concreta se
resuelve ahora la identidad del tipo léxico por módulo, namespace y dueño. Solo
si se obtiene una declaración única del proyecto se construye el tipo que
permite seguir el análisis existente de indexadores. No cambia la inferencia
general de `this`; si no se puede demostrar la identidad, se conserva la ruta
previa.

## Validación y límites

`generic_owner_indexer.py` comprueba `this[index]` desde el dueño, acceso
privado y rechazo de una selección ambigua, junto con los controles previos,
en fuente y bibliotecas estáticas/DLL con productor y consumidor O0/O2.
Pasaron además diez regresiones dirigidas. Registros y comandos en
`A:/nsc-fixes-agente-2-evidencia/generic-indexer-this-fix/`.

El compilador experimental combina tres objetos recompilados de esta rama con
bibliotecas copiadas del build de campaña; `build.log` conserva comandos y
hashes. No es un build limpio ni integrado. Falta repetir las pruebas con el
compilador de la rama principal. No se afirma cobertura para dueños cuya
identidad no pueda resolverse, ni para otras expresiones basadas en `this`.
Verificación integrada, 2026-11-11: la compilación incremental y
`ctest --test-dir A:\nsc-fixes-evidencia\build -R nsc.generics.owner-method-indexer -V`
pasaron, con 1/1 prueba ejecutada. El registro reproducible está en
`A:\nsc-fixes-evidencia\generic-indexer-this-20261111.log`. El compilador
incluye otros cambios locales y no acredita un árbol limpio. El agente informó
que el log `generic-indexer-this-fix/generic_owner_indexer.log` de su pasada
anterior fue sobrescrito por otra reproducción; no se usa como evidencia de
esta validación integrada.
