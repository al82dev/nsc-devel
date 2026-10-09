# Máquinas de estado contractuales

## Criterio

Una máquina aparece aquí sólo cuando el comportamiento observable incluye un ciclo, transición o publicación
de estado estable. Un helper puro, value object o consulta sin ciclo propio se clasifica como
`STATELESS_OR_VALUE_CONTRACT`; no se inventa una máquina para duplicar cada `if` de la implementación.

La fuente legible por máquina es `inventory/state-machines.csv`. La relación completa entre contratos y máquinas
está en `inventory/state-machine-contract-map.csv`.

## Resumen

| ID | Máquina | Contratos relacionados |
|---|---|---:|
| `SM-01-CURSOR-RECOVERY` | Reconocimiento por cursor y recuperación | 56 |
| `SM-02-DIAGNOSTIC-ACCUMULATOR` | Acumulación de diagnósticos y first-error latch | 22 |
| `SM-03-TRANSACTIONAL-PUBLICATION` | Publicación transaccional de artefactos | 47 |
| `SM-04-CACHE-LIFECYCLE` | Identidad, construcción e invalidación de caché | 13 |
| `SM-05-OWNED-RESOURCE-LIFECYCLE` | Ownership, recursos y lifetime | 49 |
| `SM-06-GRAPH-EVOLUTION` | Construcción, transformación y verificación de grafos | 128 |
| `SM-07-PIPELINE-TRANSFORMATION` | Transformación por etapas del compilador | 71 |
| `SM-08-ASYNC-JOB-LIFECYCLE` | Scheduling, concurrencia y cancelación de jobs | 24 |
| `SM-09-PROCESS-LIFECYCLE` | Preparación, supervisión y salida de procesos | 12 |
| `SM-10-SNAPSHOT-VERSION-LIFECYCLE` | Documentos, snapshots e índices versionados | 5 |
| `SM-11-CONFIGURATION-RESOLUTION` | Defaults, overrides, validación y selección | 29 |
| `SM-12-BUILDER-VERIFIER-LIFECYCLE` | Builder/verifier reutilizable y reset | 59 |

Hay 515 relaciones sobre 404 contratos con máquina; 466 contratos no poseen ciclo propio. Un contrato puede
participar en varias máquinas sin que éstas se fusionen.

## SM-01 — Cursor y recuperación

Estados: `positioned -> recognizing -> accepted|diagnosed -> recovering -> positioned|EOF`.

- invariante: todo camino consume, avanza, sincroniza o termina;
- error: el diagnóstico pertenece al rango reconocido y la recuperación ocurre sólo en un límite declarado;
- oráculo: secuencia exacta de cursor, rango, diagnóstico y progreso.

## SM-02 — Acumulador de diagnósticos

Estados: `empty -> collecting -> suppressed|failed -> reset -> empty`.

- invariante: counts, first error y suppression permanecen coherentes;
- error: el primer error no es sustituido por fallos derivados;
- oráculo: registros, conteos, primer error, summary y estado posterior a reset.

## SM-03 — Publicación transaccional

Estados: `absent -> staged -> preflighted -> committing -> published`, con salida
`rolling_back -> failed`.

- invariante: ningún consumidor observa un conjunto parcialmente aceptado;
- error: rollback o conservación del estado publicado anterior;
- oráculo: snapshot de filesystem/manifiesto antes, durante el fallo y después.

## SM-04 — Caché

Estados: `empty -> miss -> building -> ready -> hit`, con `invalidated` y `failed`.

- invariante: key, identidad de productor y revisión seleccionan el valor;
- error: una construcción fallida nunca se convierte en hit ready;
- oráculo: hit/miss/build, identidad del objeto, metadata e invalidación.

## SM-05 — Ownership y recursos

Estados: `uninitialized -> owned -> borrowed|transferred -> released|destroyed`, con `failed`.

- invariante: propietario único cuando aplica, borrow no sobrevive al owner y cleanup queda balanceado;
- error: unwind libera exactamente lo adquirido, sin leak ni double release;
- oráculo: identidades, límites de lifetime, contadores y orden de cleanup.

## SM-06 — Evolución de grafos

Estados: `empty -> discovering -> linked -> ordered -> transformed -> verified|rejected`.

- invariante: endpoints, índices, predecessor/successor, reachability y orden concuerdan;
- error: una estructura mal formada no se publica como aceptada;
- oráculo: modelo independiente de nodos, aristas, orden, dominancia o SSA según el contrato.

## SM-07 — Pipeline

Estados: `input -> admitted -> lowered -> optimized -> verified -> emitted`, con `failed` en cada frontera.

- invariante: cada consumidor recibe únicamente una salida válida del productor y conserva provenance;
- error: el pipeline se detiene en la etapa propietaria y no fabrica éxito downstream;
- oráculo: resultado, diagnóstico, provenance y conjunto exacto de productos por etapa.

## SM-08 — Job asíncrono

Estados: `created -> queued -> running -> completed|failed|cancelling -> discarded|joined`.

- invariante: dependencias preceden consumidores, resultados conservan índice y cancelación es monotónica;
- error: arbitraje determinista y descarte de resultados no publicables;
- oráculo: orden parcial permitido, identidades de worker y estado visible final.

## SM-09 — Proceso

Estados: `prepared -> launched -> running -> exited|faulted|timed_out -> collected`.

- invariante: argumentos, streams y exit/fault mantienen atribución al proceso;
- error: el mapeo de plataforma ocurre una vez y no reemplaza el exit real;
- oráculo: argv, stdout, stderr, tiempo/fault y código de salida exacto.

## SM-10 — Snapshot versionado

Estados: `absent -> opened -> updated -> analyzing -> valid|invalid -> closed`.

- invariante: versiones monotónicas y snapshot inmutable después de publicación;
- error: diagnóstico actual y último modelo válido sólo donde está declarado;
- oráculo: versión, bytes del documento, identidad de índice y secuencia de diagnósticos.

## SM-11 — Resolución de configuración

Estados: `defaults -> parsed -> merged -> validated -> resolved|rejected`.

- invariante: precedencia explícita; unknown, conflicto o fuera de rango no gana silenciosamente;
- error: rechazo en la frontera propietaria, sin fallback oculto;
- oráculo: valor resuelto, provenance de cada opción y diagnóstico exacto por categoría.

## SM-12 — Builder/verifier reutilizable

Estados: `idle -> building -> built -> verifying -> accepted|failed -> reset -> idle`.

- invariante: first error estable, reset elimina estado de la corrida anterior y accepted implica coherencia;
- error: no se devuelve salida parcial aceptada;
- oráculo: secuencia success/failure/reset en reutilizaciones consecutivas.

## Composición y estrategia futura

Las pruebas futuras ejercitarán una máquina propietaria de forma directa y compondrán máquinas sólo cuando el
contrato cruce una frontera real. Ejemplos: cache + job, pipeline + artifact transaction, process + diagnostics.
No se verificará el número de ramas internas ni se introducirán tags manuales por `if`; cobertura instrumental
pertenece a una fase posterior.

## Límites

Estas máquinas son modelos contractuales derivados de análisis estático. No afirman que todas las transiciones
hayan sido ejecutadas ni sustituyen los test plans por componente.
