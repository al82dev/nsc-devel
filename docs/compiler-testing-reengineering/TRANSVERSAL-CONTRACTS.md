# Contratos transversales del compilador

## Estado, alcance y autoridad

Estado: `HECHO COMPROBADO` para el inventario y `DISEÑO PROPUESTO` para la organización futura de pruebas.

Este documento consolida los 870 contratos de los 24 componentes host auditados. La fuente canónica es
`inventory/contracts.csv`, con SHA-256
`0aa9b0e0129e1bf6acab22b5dbfdb6828acfe9ff0533a08802130240c8633055`. La clasificación completa vive en
`inventory/transversal-contracts.csv`; este documento explica su vocabulario y sus consecuencias, pero no la
sustituye.

No se modificó producto, prueba histórica ni infraestructura. Tampoco se añadieron contratos o casos: los
totales permanecen en 870 contratos y 7,360 casos diseñados. La Tanda 11 únicamente hace explícitas relaciones
que atraviesan componentes.

## Regla de exhaustividad

Cada contrato aparece exactamente una vez en la matriz transversal y recibe una clasificación explícita en
seis ejes. Un eje sin relación demostrada usa `NOT_APPLICABLE`; la ausencia no se representa mediante una celda
vacía. Las etiquetas son acumulativas: un contrato puede requerir identidad, determinismo y caché al mismo
tiempo.

| Eje | Aplica | No aplica explícitamente | Total |
|---|---:|---:|---:|
| Diagnósticos y recuperación | 820 | 50 | 870 |
| Identidad, determinismo y caché | 704 | 166 | 870 |
| Ownership, lifetime y transferencia | 615 | 255 | 870 |
| ABI, metadata y artefactos | 544 | 326 | 870 |
| Cancelación, concurrencia y ordenamiento | 611 | 259 | 870 |
| Opciones, modos y matrices | 713 | 157 | 870 |

Los conteos miden contratos, no etiquetas. Por ello cada fila suma 870 aunque dentro del eje una fila pueda
contener varias etiquetas.

## Eje de diagnósticos y recuperación

Vocabulario controlado:

- `NO_DECLARED_ERROR`: el contrato no declara un canal de error propio;
- `DECLARED_ERROR_CHANNEL`: existe rechazo, diagnóstico, propagación o estado de fallo observable;
- `DIAGNOSTIC_EMISSION`: el contrato produce o transporta diagnóstico;
- `FIRST_ERROR_LATCH`: conserva el primer error hasta reset;
- `RECOVER_AND_CONTINUE`: consume o sincroniza y continúa desde un límite definido;
- `ROLLBACK_OR_NO_PUBLICATION`: el fallo no deja un resultado parcial aceptado;
- `FALLBACK_OR_LAST_VALID`: un fallback declarado o el último modelo válido permanece disponible;
- `SENTINEL_OR_OPTIONAL`: el fallo de consulta se expresa mediante sentinel, optional o vacío;
- `PROPAGATION`: el productor propaga una excepción o fallo de asignación;
- `STOP_AT_BOUNDARY`: el fallo termina en la frontera propietaria.

La clasificación detallada y el oráculo de cada contrato están en `inventory/error-taxonomy.csv` y se explican
en `ERROR-TAXONOMY.md`.

## Eje de identidad, determinismo y caché

Vocabulario controlado:

- `IDENTITY`: IDs, ordinals, provenance o claves deben conservar significado estable;
- `DETERMINISM`: entradas equivalentes producen orden, texto o estructura equivalentes;
- `CACHE`: existe lookup, hit/miss, almacenamiento, invalidación o interning;
- `FINGERPRINT_OR_HASH`: digest o fingerprint forma parte del resultado observable;
- `UNIQUENESS_OR_DEDUPLICATION`: duplicados, colisiones o unicidad cambian la aceptación.

La prueba futura no debe confundir identidad física con equivalencia semántica. Cuando el contrato exige pointer
identity se compara identidad; cuando exige contenido estable se usa un oráculo independiente de contenido.

## Eje de ownership, lifetime y transferencia

Vocabulario controlado:

- `OWNERSHIP`: propietario, ownership o unicidad de posesión observable;
- `BORROW_OR_LIFETIME`: validez temporal, préstamo u obligación de mantener dependencias vivas;
- `TRANSFER_MOVE_OR_COPY`: copy, clone, move o transferencia explícita;
- `RESOURCE_LIFECYCLE`: acquire/release, handles, destrucción o cleanup;
- `ALLOCATION_OR_ARENA`: arena, heap, stack o fallo de asignación;
- `SCOPE_OR_CLEANUP`: restauración de scope, unwind o acciones balanceadas.

Este eje reúne dos planos que las pruebas deberán distinguir: semántica del lenguaje y recursos propios del
host. Compartir vocabulario no autoriza usar el mismo fixture ni el mismo oráculo.

## Eje de ABI, metadata y artefactos

Vocabulario controlado:

- `ABI_OR_LAYOUT`: calling convention, layout, alignment, offsets o bit-fields;
- `METADATA_OR_SCHEMA`: NSMETA, catálogos, sidecars o schemas versionados;
- `ARTIFACT_OR_PUBLICATION`: manifiestos, archivos, bibliotecas o publicación;
- `OBJECT_OR_LINK_FORMAT`: COFF, PE, object, linker, exports o import libraries;
- `RUNTIME_CONTRACT`: runtime, startup, entry point o módulos de soporte;
- `DEBUG_SYMBOL_CONTRACT`: PDB, CodeView u otra metadata de depuración.

Los oráculos futuros deben separar bytes/formato, contenido semántico y política de publicación. Que un archivo
exista no demuestra que su ABI, metadata o clausura de dependencias sea válida.

## Eje de cancelación, concurrencia y ordenamiento

Vocabulario controlado:

- `CANCELLATION`: solicitud, propagación y observación de cancelación;
- `CONCURRENCY`: workers, threads, trabajo paralelo o acceso concurrente;
- `ATOMIC_OR_SYNCHRONIZATION`: atomics, mutexes, monitors, semaphores, TLS o locks;
- `ORDERING`: secuencia canónica, waves, orden topológico o merge determinista;
- `SCHEDULING`: admisión, cola, dispatch o política de jobs;
- `WAIT_OR_DEADLINE`: wait, join, timeout, deadline o sleep.

Las pruebas de concurrencia no pueden basarse en sleeps como oráculo. Deben usar barreras o colaboradores
controlados y comprobar estados y orden parcial permitido.

## Eje de opciones, modos y matrices

Vocabulario controlado:

- `OPTIONS_OR_FLAGS`: argumentos, opciones, flags y precedencia;
- `MODE_OR_PROFILE`: modos, perfiles, subsystem o worker mode;
- `OPTIMIZATION_CONFIGURATION`: O0-O3, LTO, pipeline u opciones de optimización;
- `TARGET_OR_PLATFORM`: target, CPU, arquitectura, triple o plataforma;
- `LANGUAGE_OR_FEATURE_POLICY`: capability, compatibilidad o feature policy;
- `ENVIRONMENT_OR_TOOLCHAIN`: SDK, toolchain, environment o rutas externas;
- `MATRIX_RANGE_OR_LIMIT`: versiones, budgets, rangos y límites.

Cada matriz futura debe declarar dimensiones y equivalencias. No se multiplican configuraciones que no cambien
el contrato, pero tampoco se colapsan dos configuraciones con resultados observables distintos.

## Relación con máquinas de estado

De los 870 contratos:

- 404 participan en una o más máquinas de estado estables;
- 466 se clasifican como `STATELESS_OR_VALUE_CONTRACT`;
- existen 515 relaciones contrato-máquina;
- existen 12 máquinas normalizadas.

La diferencia entre 404 contratos y 515 relaciones es intencional. Una generación de artefacto puede atravesar
configuración, pipeline, recurso y transacción sin que esos ciclos se fusionen en una sola máquina monolítica.
El detalle vive en `STATE-MACHINES.md`, `inventory/state-machines.csv` e
`inventory/state-machine-contract-map.csv`.

## Consecuencias para la Fase 2

1. Los casos de componente conservan el contrato propietario.
2. Los escenarios transversales enlazan casos existentes; no duplican unitarias por cada cruce.
3. Los fallos se inyectan en la frontera productora, nunca en consumidores derivados.
4. Los oráculos comparan estados y artefactos estables, no conteos manuales de ramas internas.
5. Las matrices de configuración se derivan de diferencias observables, no de combinaciones cartesianas ciegas.
6. Self-host y procesos costosos quedan fuera del circuito rápido salvo que sean el único oráculo del contrato.

## Límites de la afirmación

La clasificación es una auditoría estática de la revisión congelada. No demuestra ejecución, cobertura de línea,
cobertura de rama, ausencia de defectos ni implementación de la suite futura.
