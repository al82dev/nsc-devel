# Clausura de indexadores con retornos genéricos distintos

## Reproducción y cambio

En `e8d9f0189`, `GenericLookup<N>` declara getters `int` y `long` que
devuelven respectivamente `Fixture.Own<N>` y `Wrong.Own<N>`. Ambos tipos
declaran un método `At` distinto. Desde fuentes, llamar las dos rutas devuelve
42 en O0/O2; publicar el productor rechaza la clausura. Las fuentes intactas,
comandos y resultados están en
`A:/nsc-fixes-agente-2-evidencia/indexer-distinct-closure-before/`.

El colector registra ahora los métodos de todos los dueños de retorno cuando
cada getter pertinente identifica una declaración genérica exacta sin bases y
cada dueño declara un método aplicable. No selecciona ni modifica el indexador:
la compilación posterior decide cuál se llama. Si falta información, hay varias
sobrecargas y algún getter es complejo, o algún dueño carece del método,
conserva la ruta anterior.

## Pruebas y límites

La misma reproducción publica y ejecuta desde fuentes O0/O2 con el compilador
experimental; véase `indexer-distinct-closure-after/results.json`. La regresión
`generic_owner_indexer.py` comprueba también publicación y ejecución por
biblioteca estática y DLL, productor y consumidor O0/O2, ejercitando los dos
getters y sus métodos homónimos. Pasaron diez regresiones dirigidas; los logs
están en `generic-indexer-distinct-closure-fix/` bajo la carpeta de evidencia.

Si uno de los retornos carece de `At`, el productor del control sigue
publicándose mediante el fallback anterior. El consumidor se rechaza tanto al
llamar la ruta sin método como al llamar solo la otra, porque la
especialización procesa también el cuerpo que falla. Resultados:
`indexer-distinct-closure-after/missing-results.json`,
`missing-consumer-results.json` y `present-consumer-results.json`. No se
considera resuelta esa limitación ni se atribuye la dependencia ajena como
válida. La prueba dirigida conserva el control sin exigir que una futura
corrección continúe publicando ese productor.

El compilador es híbrido: tres objetos de esta copia y bibliotecas del build
de campaña; `build.log` conserva comandos y hashes. Falta la validación con
el compilador integrado. No se afirma cobertura de herencia, parámetros
complejos o resolución general de sobrecargas.
Verificación integrada, 2026-11-11: la compilación incremental y la prueba
`nsc.generics.owner-method-indexer` pasaron; CTest ejecutó 1/1 sin omisión.
El log `A:\nsc-fixes-evidencia\generic-indexer-distinct-metadata-20261111.log`
incluye los controles y rechazos esperados. La ejecución usa un compilador con
otros cambios locales y no certifica una revisión limpia; el caso de método
ausente parcialmente seleccionado sigue pendiente según se explica abajo.
