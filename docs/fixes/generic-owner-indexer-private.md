# Indexadores privados en inferencia de dueño

## Reproducción

En la revisión `56c861ab5`, `PrivateCandidate` declara un indexador público
`short` que devuelve `Own<0>` y otro privado `ushort` que devuelve `Own<1>`.
Desde otro tipo, el acceso ordinario con `byte` selecciona el público y
devuelve 42, pero la llamada directa `items[index].At<42, 0>()` falla antes de
la resolución semántica. También falla cuando el tipo que llama se llama
`PrivateCandidate` en otro namespace. Se reprodujo en O0 y O2; registro:
`A:/nsc-fixes-agente-2-evidencia/generic-indexer-diagnostic-fix/generic_owner_indexer.log`.
Esta ejecución conserva controles de acceso del propio tipo y rechazo externo.

## Causa y cambio

`ProjectGenericModel::processGenericCall` reunía ambos candidatos sin tener en
cuenta el acceso privado. En la selección anticipada ahora se excluye el
getter marcado privado cuando se identifica inequívocamente el tipo que llama
en el proyecto y es distinto de la declaración ordinaria dueña del indexador.
La comparación utiliza identidad de sintaxis, no igualdad de nombre simple.
El caso de dueño genérico y toda identidad desconocida conservan la ruta
anterior; la resolución semántica definitiva sigue aplicando accesibilidad.

## Evidencia y límites

La prueba `generic_owner_indexer.py` comprueba el acceso público externo,
un tipo homónimo en otro namespace, el getter privado desde el propio dueño y
el rechazo desde fuera cuando solo existe el privado o cuando este es la
coincidencia exacta. Ejecuta fuente y bibliotecas estáticas/DLL con productor
y consumidor O0/O2. Registro y regresiones adicionales:
`A:/nsc-fixes-agente-2-evidencia/generic-indexer-private-final/`.

El ejecutable experimental enlaza tres objetos recompilados de esta rama con
bibliotecas copiadas del build de campaña; comandos y hashes están en su
`build.log`. No es un build limpio ni integrado. Deben repetirse las pruebas
con el compilador de la rama principal. No se afirma que esta corrección cubra
la resolución de acceso para dueños genéricos o de identidad desconocida.
Verificación integrada, 2026-11-11: la compilación incremental de `nsc` y
`ctest --test-dir A:\nsc-fixes-evidencia\build -R nsc.generics.owner-method-indexer -V`
pasaron; CTest ejecutó 1/1 sin omisiones. El registro completo es
`A:\nsc-fixes-evidencia\generic-indexer-private-20261111.log`. El compilador
contenía otros cambios locales, por lo que no acredita un árbol limpio.
