# Taxonomía transversal de errores y recuperación

## Universo y fuente

`inventory/error-taxonomy.csv` contiene exactamente una fila por cada uno de los 870 contratos. Conserva el
texto de error declarado, clases normalizadas, política de recuperación, oráculo y procedencia exacta.

- contratos con canal de error declarado: 820;
- contratos sin error directo declarado: 50;
- filas vacías, duplicadas o sin contrato fuente: 0.

Las clases y políticas son acumulativas; sus conteos no deben sumarse para inferir el total de contratos.

## Clases de error

| Clase | Contratos | Significado de prueba |
|---|---:|---|
| `VALIDATION_OR_SCHEMA` | 543 | Entrada, estructura o formato incumple una regla verificable |
| `INVARIANT_OR_VERIFICATION` | 188 | Un modelo interno no satisface invariantes estructurales |
| `SEMANTIC_OR_TYPE` | 140 | Programa reconocido pero semánticamente inválido o incompatible |
| `DECLARED_DOMAIN_FAILURE` | 127 | Fallo propio del dominio que no pertenece a otra clase más estrecha |
| `BUDGET_OR_LIMIT` | 95 | Límite, tamaño, profundidad, capacidad o rango excedido |
| `PROCESS_OR_PLATFORM` | 94 | Proceso, OS, fault boundary o capacidad de plataforma |
| `CANCELLATION_OR_TIMEOUT` | 82 | Cancelación, deadline o timeout observable |
| `SYNTAX_OR_PARSE` | 75 | Token, gramática, parse o recuperación sintáctica |
| `TOOLCHAIN_OR_BACKEND` | 56 | LLVM, linker, backend u otra herramienta productora |
| `IO_FILESYSTEM_OR_ARTIFACT` | 51 | Lectura, escritura, path, stream o conjunto de artefactos |
| `NO_DECLARED_ERROR` | 50 | No existe canal de error propio en el contrato |
| `RESOURCE_OR_ALLOCATION` | 48 | Asignación, memoria, handle o recurso no disponible |

Un error de productor no se reetiqueta como defecto del consumidor. Si el backend no produce un objeto válido,
las pruebas posteriores quedan `BLOQUEADO_POR` ese productor en lugar de usar un artefacto viejo o un fallback
silencioso.

## Políticas de recuperación

| Política | Contratos | Oráculo mínimo |
|---|---:|---|
| `STOP_AT_BOUNDARY` | 305 | La frontera propietaria falla y no reporta éxito downstream |
| `ROLLBACK_OR_NO_PUBLICATION` | 290 | No queda resultado parcial o stale aceptado |
| `FALLBACK_OR_LAST_VALID` | 188 | Sólo el fallback expresamente declarado permanece observable |
| `SENTINEL_OR_OPTIONAL` | 154 | Sentinel/optional/vacío exacto, sin mutación escondida |
| `RECOVER_AND_CONTINUE` | 52 | Progreso posterior, diagnóstico conservado y estado sincronizado |
| `NOT_APPLICABLE` | 50 | No existe recuperación porque no existe error directo |
| `PROPAGATE` | 22 | El fallo conserva tipo, frontera y cleanup al propagarse |
| `FIRST_ERROR_LATCH` | 21 | El primer error permanece estable hasta reset explícito |

## Diseño del oráculo

Una prueba de error futura debe declarar por separado:

1. entrada y precondición que activan el fallo;
2. productor responsable;
3. clase de error;
4. estado observable posterior;
5. artefactos que no deben existir o cambiar;
6. posibilidad o prohibición de reuso;
7. diagnóstico estable que constituye contrato.

No se congelará puntuación, espacios o redacción incidental sólo porque aparezcan en una salida actual. El texto
se compara byte a byte únicamente cuando el contrato documentado define un formato público exacto, un dump
canónico o una identidad consumida por herramientas. En los demás casos se comprueban código, rango, categoría,
payload y recuperación.

## Patrones de recuperación por subsistema

- Lexer y Parser: diagnóstico localizado, progreso y sincronización declarada.
- Builders y verifiers: first-error latch, resultado no aceptado y reset entre reutilizaciones.
- Grafos y pipelines: detener el consumidor posterior y conservar provenance del productor fallido.
- Cachés: nunca publicar hit ready a partir de una construcción fallida o stale.
- Artefactos: staging, preflight, commit o rollback sin conjuntos parciales aceptados.
- LanguageService: diagnóstico actual con último modelo válido sólo donde el contrato lo permite.
- Jobs y procesos: cancelación/fault/exit se arbitran una vez y no se sustituyen por éxito.
- Recursos: todo acquire observado queda balanceado al fallar.

## Límites

La taxonomía describe el código auditado. No corrige errores, no estandariza mensajes del compilador y no afirma
que las políticas hayan sido ejecutadas.
