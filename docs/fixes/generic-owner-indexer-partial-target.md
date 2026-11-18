# Getter no seleccionado y método genérico ausente

## Reproducción y controles

Con el compilador experimental posterior a `9752cdd6b`, una estructura
`Fixture.Own<N>` declara `At`; `Wrong.Own<N>` no. Un tipo genérico tiene
indexadores `int` y `long` que devuelven esos tipos respectivamente. Los
cuatro proyectos y sus comandos están en
`A:/nsc-fixes-agente-2-evidencia/indexer-partial-target-probe/results.json`.
En todos los casos se probaron O0 y O2 desde fuentes y como biblioteca estática.

| Cuerpo de la plantilla y consumidor | Fuentes | Productor | Consumidor binario |
| --- | --- | --- | --- |
| Solo `ReadInt`, llama `this[0].At` | Ejecuta 42 | Publica | Ejecuta 42 |
| Solo `ReadLong`, llama `this[longLocal].At` | Rechazado | Publica | Rechazado |
| Ambos cuerpos, consumidor llama `ReadInt` | Rechazado | Publica | Rechazado |
| Ambos cuerpos, consumidor llama `ReadLong` | Rechazado | Publica | Rechazado |

Los diagnósticos y las fuentes exactas, incluidos el segundo cuerpo no
seleccionado y el código del consumidor, se conservan en el registro. No se
puede rechazar el productor por la sola presencia de un getter cuyo tipo no
tiene `At`: eso rechazaría el primer caso válido. Tampoco se considera
validado el productor del segundo caso por haber emitido metadata.

## Límite de información

`ProjectGenericModel::collectCallDependency` recorre la sintaxis antes de la
resolución semántica de sobrecargas y, en la ruta examinada, no incorpora las
declaraciones locales al `GenericLookupScope`. La fase que especializa la
llamada puede resolver el caso al materializar el cuerpo; el colector de
clausura no tiene aquí una decisión equivalente demostrada para `longLocal`.
Tratar todos los getters como seleccionados o elegir uno solo por su nombre
produciría un rechazo o una dependencia injustificados.

Queda pendiente estudiar cómo compartir una selección semántica comprobable o
aplazar su comprobación sin ocultar la dependencia binaria. Esa investigación
debe cubrir literales, locales, conversiones, ambigüedad, accesibilidad,
sobrecargas y el caso con un solo cuerpo válido. No se cambió el compilador en
esta evaluación. El ejecutable de prueba mezcla objetos recompilados con
bibliotecas del build de campaña y no certifica una compilación limpia ni
integrada.

## Investigación posterior de selección

`ProjectCompilation::compileProjectNames` ejecuta previamente la fase de
genéricos; miembros y cuerpos se construyen después. Por ello el binding de
sobrecarga del modelo de cuerpos aún no está disponible al recolectar estas
dependencias. El orden consta en `src/Frontend/ProjectCompilation.cpp` y el
comparador semántico en `src/Frontend/Bodies/OverloadCandidateRanking.cpp`.

Los controles adicionales sustituyen el índice por `(int)0` y `(long)0`. El
primero publica y ejecuta 42; el segundo publica pero su consumidor se rechaza,
en O0/O2. Comandos y diagnósticos:
`A:/nsc-fixes-agente-2-evidencia/indexer-partial-target-casts/results.json`.
La inferencia genérica puede conocer el tipo escrito del cast, pero resolver
solo esa forma duplicaría una parte de la selección sin resolver el local
original. Se conserva pendiente una vía que utilice bindings o ámbitos
correctos; no se ha implementado un atajo para el cast.
