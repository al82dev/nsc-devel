# Test plan — Host.Diagnostics

## Estado

- **Clasificación:** `DISEÑO PROPUESTO` basado en la implementación congelada.
- **Pruebas creadas o ejecutadas:** ninguna.
- **Oráculos:** modelo, conteos, orden y bytes de salida documentados en `HOST-DIAGNOSTICS.md`.

Los casos lógicos pueden parametrizarse dentro del futuro módulo unitario consolidado. `PR` cubre memoria y
streams deterministas; `NIGHTLY` añade shards/permutaciones y escenarios de gran salida.

## Contratos

| Contrato | Superficie | Riesgo principal |
|---|---|---|
| CD-DIA-001 | severidad y nombres | spelling/fallback inestable |
| CD-DIA-002 | catálogo y nombres de ID | código machine-facing roto |
| CD-DIA-003 | ownership de `Diagnostic` | datos colgantes o alterados |
| CD-DIA-004 | reportes y conteos | severidad/rango mal almacenado |
| CD-DIA-005 | límite y supresión | off-by-one o severidad posterior aceptada |
| CD-DIA-006 | append | pérdida de datos o presupuesto duplicado |
| CD-DIA-007 | merge | orden dependiente de shards |
| CD-DIA-008 | consultas y clear | estado residual |
| CD-DIA-009 | encabezado | formato CLI/IDE roto |
| CD-DIA-010 | línea y marcador | caret/longitud incorrectos |
| CD-DIA-011 | labels y notes | evidencia omitida |
| CD-DIA-012 | renderAll/resumen | orden o conteo engañoso |

## Vocabulario y modelo

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TD-DIA-001 | CD-DIA-001 | cuatro severidades | nombres exactos note/warning/error/lint | PR |
| TD-DIA-002 | CD-DIA-001 | valor fuera del enum | fallback `error` | PR |
| TD-DIA-003 | CD-DIA-002 | los 74 IDs | cada nombre exacto y único según tabla | PR |
| TD-DIA-004 | CD-DIA-002 | primeros 69 IDs | `isNativeSharp3Diagnostic=false` | PR |
| TD-DIA-005 | CD-DIA-002 | cinco IDs NS | true y nombres cortos exactos | PR |
| TD-DIA-006 | CD-DIA-002 | valor de ID desconocido | `UnknownDiagnostic` y false | PR |
| TD-DIA-007 | CD-DIA-003 | diagnóstico sólo con ubicación | conserva ID severidad ubicación mensaje y rango ausente | PR |
| TD-DIA-008 | CD-DIA-003 | diagnóstico con rango | rango exacto por valor | PR |
| TD-DIA-009 | CD-DIA-003 | labels y notes temporales | instancia posee copias/movimientos válidos tras morir entradas | PR |
| TD-DIA-010 | CD-DIA-003 | label con rango y mensaje | ambos campos conservados | PR |
| TD-DIA-011 | CD-DIA-003 | ubicación y rango deliberadamente incoherentes | modelo los conserva sin validar | PR |

## Reporte, conteos y límite

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TD-DIA-012 | CD-DIA-004 | engine nuevo | vector y conteos cero; sin errores; límite 100 | PR |
| TD-DIA-013 | CD-DIA-004 | report Error por ubicación | rango ausente; errores 1; mensaje exacto | PR |
| TD-DIA-014 | CD-DIA-004 | report Warning por rango | ubicación igual a begin; rango presente; warnings 1 | PR |
| TD-DIA-015 | CD-DIA-004 | report Lint | lints 1 | PR |
| TD-DIA-016 | CD-DIA-004 | report Note | almacenado; los tres conteos permanecen cero | PR |
| TD-DIA-017 | CD-DIA-004 | reportStructured | rango labels notes y mensaje exactos | PR |
| TD-DIA-018 | CD-DIA-004 | mezcla de severidades bajo límite | orden de inserción y conteos independientes | PR |
| TD-DIA-019 | CD-DIA-005 | límite solicitado cero | límite efectivo 1 | PR |
| TD-DIA-020 | CD-DIA-005 | límite 1 y primer Error | error almacenado; reached true; suprimidos 0 | PR |
| TD-DIA-021 | CD-DIA-005 | segundo Error después del límite | no almacenado; suprimidos 1 | PR |
| TD-DIA-022 | CD-DIA-005 | Warning después del límite | no almacenado ni contado; suprimidos aumenta | PR |
| TD-DIA-023 | CD-DIA-005 | Lint después del límite | mismo contrato de supresión global | PR |
| TD-DIA-024 | CD-DIA-005 | Note después del límite | mismo contrato de supresión global | PR |
| TD-DIA-025 | CD-DIA-005 | límite 3 y dos errores | reached false | PR |
| TD-DIA-026 | CD-DIA-005 | tercer error con límite 3 | se almacena y después reached true | PR |

## Append, merge y reset

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TD-DIA-027 | CD-DIA-006 | append simple sin rango | copia exacta y aplica conteo receptor | PR |
| TD-DIA-028 | CD-DIA-006 | append con rango | rango y ubicación begin preservados | PR |
| TD-DIA-029 | CD-DIA-006 | append estructurado con rango | labels notes y rango preservados | PR |
| TD-DIA-030 | CD-DIA-006 | append estructurado sin rango | sintetiza rango vacío en location | PR |
| TD-DIA-031 | CD-DIA-006 | append cuando receptor alcanzó límite | suprime y no copia | PR |
| TD-DIA-032 | CD-DIA-007 | shards en dos órdenes con claves distintas | secuencia final idéntica | PR |
| TD-DIA-033 | CD-DIA-007 | ubicación válida e inválida | válida ordena primero | PR |
| TD-DIA-034 | CD-DIA-007 | distintos FileId/start/end | orden lexicográfico exacto | PR |
| TD-DIA-035 | CD-DIA-007 | misma ubicación y cuatro severidades | Error Warning Lint Note | PR |
| TD-DIA-036 | CD-DIA-007 | mismo lugar/severidad con ID distinto | orden entero del enum | PR |
| TD-DIA-037 | CD-DIA-007 | mismo lugar/ID con mensajes distintos | orden lexicográfico de mensaje | PR |
| TD-DIA-038 | CD-DIA-007 | rango ausente | end key usa offset de location | PR |
| TD-DIA-039 | CD-DIA-007 | receptor con diagnóstico previo | previo permanece y merged se agrega después del sort local | PR |
| TD-DIA-040 | CD-DIA-007 | límite receptor menor que entradas | presupuesto se aplica una vez y resto se suprime | PR |
| TD-DIA-041 | CD-DIA-007 | shards con suprimidos | conteo final suma shard y receptor | PR |
| TD-DIA-042 | CD-DIA-007 | clave idéntica pero labels distintos en permutaciones | expone orden no total; no declarar determinismo si difiere | NIGHTLY |
| TD-DIA-043 | CD-DIA-008 | consultas tras mezcla de reportes | cada accessor refleja estado almacenado | PR |
| TD-DIA-044 | CD-DIA-008 | clear tras errores y suprimidos | vector y conteos cero; flags false | PR |
| TD-DIA-045 | CD-DIA-008 | clear sobre límite configurado 7 | `errorLimit` sigue 7 | PR |
| TD-DIA-046 | CD-DIA-008 | reportar después de clear | acepta de nuevo hasta el mismo límite | PR |

## Encabezado y marcador

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TD-DIA-047 | CD-DIA-009 | fuente válida e ID normal | ruta:línea:columna severidad mensaje; sin código | PR |
| TD-DIA-048 | CD-DIA-009 | ubicación inválida | `<source>:1:1` | PR |
| TD-DIA-049 | CD-DIA-009 | cada severidad | nombre exacto en encabezado | PR |
| TD-DIA-050 | CD-DIA-009 | cada uno de cinco IDs NS | `[NSxxxx]` sin espacio tras severidad | PR |
| TD-DIA-051 | CD-DIA-009 | ID normal y desconocido | no imprime corchetes/código | PR |
| TD-DIA-052 | CD-DIA-009 | mensaje vacío o con puntuación | bytes del mensaje conservados y newline final | PR |
| TD-DIA-053 | CD-DIA-010 | línea ASCII columna 1 sin rango | línea indentada y `^` | PR |
| TD-DIA-054 | CD-DIA-010 | columna posterior con espacios | padding de espacios antes de caret | PR |
| TD-DIA-055 | CD-DIA-010 | tab antes del caret | tab replicado no expandido | PR |
| TD-DIA-056 | CD-DIA-010 | rango mismo renglón de longitud 1 | sólo `^` | PR |
| TD-DIA-057 | CD-DIA-010 | rango mismo renglón longitud N | `^` más N-1 tildes | PR |
| TD-DIA-058 | CD-DIA-010 | rango vacío | marcador de longitud 1 | PR |
| TD-DIA-059 | CD-DIA-010 | rango multilínea | marcador de longitud 1 | PR |
| TD-DIA-060 | CD-DIA-010 | rango termina después del texto visible | longitud no se clampa | PR |
| TD-DIA-061 | CD-DIA-010 | línea vacía columna 1 | imprime línea vacía y caret | PR |
| TD-DIA-062 | CD-DIA-010 | línea vacía columna distinta de 1 | sólo encabezado y retorno temprano | PR |
| TD-DIA-063 | CD-DIA-010 | modelo directo con location/range incoherentes | caracteriza riesgo de resta unsigned sin ejecutar salida descontrolada | NIGHTLY |

## Etiquetas, notas y renderAll

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TD-DIA-064 | CD-DIA-011 | una etiqueta misma fuente | ruta línea columna note y mensaje | PR |
| TD-DIA-065 | CD-DIA-011 | etiqueta en otra fuente | usa ruta/posición de su propio rango | PR |
| TD-DIA-066 | CD-DIA-011 | etiqueta con archivo inválido | fallback `<source>:1:1` | PR |
| TD-DIA-067 | CD-DIA-011 | dos labels y dos notes | conserva orden de cada vector | PR |
| TD-DIA-068 | CD-DIA-011 | notes sin labels | cada una `note: texto` | PR |
| TD-DIA-069 | CD-DIA-011 | línea primaria vacía y columna distinta de 1 | retorno temprano omite labels y notes según conducta vigente | PR |
| TD-DIA-070 | CD-DIA-012 | engine vacío | salida vacía | PR |
| TD-DIA-071 | CD-DIA-012 | tres diagnósticos | renderiza exactamente en orden del vector | PR |
| TD-DIA-072 | CD-DIA-012 | sin supresiones | no imprime resumen | PR |
| TD-DIA-073 | CD-DIA-012 | límite tras N errores y M suprimidos | resumen exacto con N y M | PR |
| TD-DIA-074 | CD-DIA-012 | un error y un suprimido | conserva literal plural `errors` | PR |
| TD-DIA-075 | CD-DIA-012 | stream con excepción habilitada | excepción del stream se propaga; estado del engine no cambia | NIGHTLY |

## Reglas de oráculo

- La salida se compara byte a byte, incluidos espacios, tabs, corchetes y saltos.
- El catálogo de 74 IDs se deriva de una tabla independiente del `switch` bajo prueba.
- El merge se prueba con todas las permutaciones de un conjunto pequeño y con semillas reproducibles mayores.
- Los empates no cubiertos por la clave se reportan como riesgo, no se normalizan en el test.
- Los casos de modelo incoherente aíslan el riesgo sin intentar reservar o emitir una cantidad descontrolada.

## Cierre futuro

El componente sólo podrá considerarse verificado cuando los 75 casos estén implementados o justificados por
fila, los 74 IDs tengan oráculo independiente, las permutaciones de merge se comparen y el formato visible sea
idéntico en las integraciones CLI/IDE aplicables.
