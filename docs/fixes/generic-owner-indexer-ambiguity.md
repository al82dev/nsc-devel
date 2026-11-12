# Diagnóstico de indexadores ambiguos en llamadas genéricas

## Reproducción y causa

Con `byte index` y getters públicos `this[short]` y `this[ushort]`, el
acceso ordinario se rechaza por ambigüedad. Antes de este cambio,
`items[index].At<42, 0>()` se rechazaba con un error de índice de pack, antes
de informar la ambigüedad. Los comandos y diagnósticos previos para O0/O2
están en `A:/nsc-fixes-agente-2-evidencia/generic-indexer-conversions-indexer-constant-fix/results.json`.

`ProjectGenericModel::processGenericCall` determina el dueño antes de la
resolución semántica ordinaria. Si su selección numérica ya clasificada deja
varios candidatos no dominados, ahora informa ambigüedad únicamente cuando el
conjunto observado no presenta herencia, accesores no públicos, restricciones,
referencias, parámetros opcionales o variádicos ni discrepancias de aridad.
Cuando esa condición no se cumple conserva el rechazo previo, sin escoger un
dueño ni atribuirle una ambigüedad que no pudo demostrarse.

## Validación y límites

`generic_owner_indexer.py` pasó para fuentes y consumidores de bibliotecas
estáticas y DLL, con productor y consumidor O0/O2. Exige el diagnóstico de
ambigüedad para los ejemplos directos de uno y dos argumentos; comprueba las
conversiones y los nombres nominales previos. Su control privado comprueba que
no se anuncie una ambigüedad falsa. Registro:
`A:/nsc-fixes-agente-2-evidencia/generic-indexer-diagnostic-fix/generic_owner_indexer.log`.
Las regresiones dirigidas adicionales se registran en el mismo directorio.

El control con un getter público `short` y otro privado `ushort` revela una
limitación independiente: desde fuente, el acceso ordinario devuelve 42 pero
la llamada genérica directa aún se rechaza; desde biblioteca devuelve 42
porque el getter privado no se exporta. La prueba acepta ese rechazo sin
clasificarlo como éxito funcional. La selección y el diagnóstico completos
para las formas no cubiertas siguen pendientes.

El compilador experimental enlaza tres objetos recompilados de esta rama con
bibliotecas copiadas del build de campaña; comandos y hashes figuran en
`generic-indexer-diagnostic-fix/build.log`. No demuestra un build limpio ni
integrado; falta repetir las pruebas con el compilador de la rama principal.
Verificación integrada, 2026-11-11: la compilación incremental de `nsc` pasó y
`ctest --test-dir A:\nsc-fixes-evidencia\build -R nsc.generics.owner-method-indexer -V`
pasó 1/1, sin omisión. El registro completo está en
`A:\nsc-fixes-evidencia\generic-indexer-ambiguity-20261111.log`. El compilador
contenía otros cambios locales; no acredita una compilación de revisión limpia.
