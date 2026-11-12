# Campo de un receptor indexado y método genérico

## Reproducción

En `081032d60`, `items[0].Value.At<20, 42>()` se rechaza en O0/O2 cuando
el indexador devuelve `Holder<1>` y su campo `Value` tiene tipo `Own<1>`.
Guardar el resultado en `Holder<1> holder` y llamar `holder.Value.At<...>()`
ejecuta 42. Comandos, fuentes y diagnósticos están en
`A:/nsc-fixes-agente-2-evidencia/indexer-field-receiver-before/results.json`.
Las fuentes antes y después son idénticas: SHA-256 del productor
`989c84156dda61689413a6557c9f1325c2b81f6a1636f5972fc92c6066d11fe5` y del
consumidor directo `cd83eeb8f751846ef69647527bd0242634347cc2ef3a6b5b879bdd49c51a1516`.

## Causa y corrección

La rama de `ProjectGenericModel::processGenericCall` que infiere el tipo de
un campo ya reconocía elementos de arreglos dinámicos, pero no el resultado de
un indexador de usuario. Ahora reconoce únicamente un getter público único de
un dueño ordinario sin bases y usa su tipo de retorno. La resolución del tipo
ocurre con la unidad, namespace, imports y aliases del propio indexador; la
resolución posterior de campo mantiene su contexto. Las demás formas conservan
la ruta anterior. El análisis semántico posterior mantiene la accesibilidad y
las reglas de propiedad; este cambio no modifica esos análisis.

## Pruebas y límites

El ejemplo directo y su control ejecutan 42 en O0/O2 después del cambio:
`indexer-field-receiver-after/results.json` en la carpeta de evidencia. La
regresión `generic_owner_indexer.py` también cubre tipos homónimos en otro
namespace, consumidor sin import del namespace del productor, índice genérico
inválido y campo privado rechazado, en fuentes y bibliotecas estáticas/DLL con
productor y consumidor O0/O2. Pasaron diez regresiones dirigidas adicionales;
logs en `generic-indexer-field-receiver-fix/` de la carpeta de evidencia.

El compilador experimental enlaza tres objetos de esta rama con bibliotecas
copiadas del build de campaña; `build.log` conserva comandos y hashes. Falta
la validación integrada. No se afirma cobertura de indexadores sobrecargados,
dueños genéricos o heredados ni otros tipos de acceso a campos.
Verificación integrada, 2026-11-11: la compilación incremental y
`nsc.generics.owner-method-indexer` pasaron; CTest ejecutó 1/1 sin omisiones.
El log `A:\nsc-fixes-evidencia\generic-indexer-field-20261111.log` conserva
las variantes y los rechazos esperados. El compilador contenía otros cambios
locales y no certifica una revisión limpia; los indexadores sobrecargados y
las demás formas indicadas abajo siguen fuera de esta corrección.
