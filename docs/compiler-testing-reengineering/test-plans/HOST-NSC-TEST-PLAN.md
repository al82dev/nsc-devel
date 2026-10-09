# Plan de pruebas — Host.nsc

## 1. Alcance

Plan contractual del borde de proceso. No implementa ni ejecuta pruebas.

## 2. Fixtures futuros

- argv Unicode/UTF-8, switches globales, comandos de compilación y consultas.
- Filesystem temporal para logs, claims, time traces y bundles.
- Streams que aceptan o rechazan escritura/sync.
- Supervisor LLVM simulado y Driver controlado en process fixtures.

## 3. Contratos

| ID | Contrato | Capa | Oráculo |
|---|---|---|---|
| `CF-NSC-001` | Automatic error-log policy | `unit` | boolean |
| `CF-NSC-002` | Tee stream fanout and synchronization | `unit` | bytes and status |
| `CF-NSC-003` | Compilation-log JSON escaping | `unit` | exact escaped bytes |
| `CF-NSC-004` | Atomic payload publication | `unit` | result and final bytes |
| `CF-NSC-005` | Text and JSON compilation-log schemas | `unit` | exact schema bytes |
| `CF-NSC-006` | Collision-safe automatic error-log allocation | `unit` | chosen path and bytes |
| `CF-NSC-007` | Compilation invocation classification | `unit` | boolean table |
| `CF-NSC-008` | JSON-output detection and banner suppression | `unit` | boolean and banner presence |
| `CF-NSC-009` | Project and library-root option extraction | `unit` | exact extracted paths |
| `CF-NSC-010` | Progress option parsing | `unit` | flag and format |
| `CF-NSC-011` | Global console normalization and language scope | `process` | arguments error and exit |
| `CF-NSC-012` | Verbose build progress and time normalization | `process` | exact forwarded arguments |
| `CF-NSC-013` | Log-format validation | `process` | exit and diagnostic |
| `CF-NSC-014` | LLVM trace supervisor and worker mode | `process` | exit diagnostics and observation |
| `CF-NSC-015` | Captured dispatch banner and top-level progress | `process` | ordered output and exit |
| `CF-NSC-016` | Explicit and automatic compilation logs | `process` | files warning and exit |
| `CF-NSC-017` | Compilation time-trace publication | `process` | trace JSON |
| `CF-NSC-018` | Failure diagnostic bundle publication | `process` | bundle warning and exit |
| `CF-NSC-019` | Windows UTF-16 argument conversion | `process` | exact UTF-8 or exception |
| `CF-NSC-020` | Platform entry point crash boundary | `process` | process exit and diagnostics |

## 4. Casos diseñados

| ID | Contrato | Objetivo | Frecuencia |
|---|---|---|---|
| `TF-NSC-001` | `CF-NSC-001` | nominal minimo: Automatic error-log policy | `PR` |
| `TF-NSC-002` | `CF-NSC-001` | nominal representativo: Automatic error-log policy | `PR` |
| `TF-NSC-003` | `CF-NSC-001` | prerrequisito ausente: Automatic error-log policy | `PR` |
| `TF-NSC-004` | `CF-NSC-001` | entrada invalida: Automatic error-log policy | `PR` |
| `TF-NSC-005` | `CF-NSC-001` | borde inferior: Automatic error-log policy | `PR` |
| `TF-NSC-006` | `CF-NSC-001` | borde superior: Automatic error-log policy | `PR` |
| `TF-NSC-007` | `CF-NSC-001` | repeticion y determinismo: Automatic error-log policy | `PR` |
| `TF-NSC-008` | `CF-NSC-001` | hostil y concurrencia: Automatic error-log policy | `NIGHTLY` |
| `TF-NSC-009` | `CF-NSC-002` | nominal minimo: Tee stream fanout and synchronization | `PR` |
| `TF-NSC-010` | `CF-NSC-002` | nominal representativo: Tee stream fanout and synchronization | `PR` |
| `TF-NSC-011` | `CF-NSC-002` | prerrequisito ausente: Tee stream fanout and synchronization | `PR` |
| `TF-NSC-012` | `CF-NSC-002` | entrada invalida: Tee stream fanout and synchronization | `PR` |
| `TF-NSC-013` | `CF-NSC-002` | borde inferior: Tee stream fanout and synchronization | `PR` |
| `TF-NSC-014` | `CF-NSC-002` | borde superior: Tee stream fanout and synchronization | `PR` |
| `TF-NSC-015` | `CF-NSC-002` | repeticion y determinismo: Tee stream fanout and synchronization | `PR` |
| `TF-NSC-016` | `CF-NSC-002` | hostil y concurrencia: Tee stream fanout and synchronization | `NIGHTLY` |
| `TF-NSC-017` | `CF-NSC-003` | nominal minimo: Compilation-log JSON escaping | `PR` |
| `TF-NSC-018` | `CF-NSC-003` | nominal representativo: Compilation-log JSON escaping | `PR` |
| `TF-NSC-019` | `CF-NSC-003` | prerrequisito ausente: Compilation-log JSON escaping | `PR` |
| `TF-NSC-020` | `CF-NSC-003` | entrada invalida: Compilation-log JSON escaping | `PR` |
| `TF-NSC-021` | `CF-NSC-003` | borde inferior: Compilation-log JSON escaping | `PR` |
| `TF-NSC-022` | `CF-NSC-003` | borde superior: Compilation-log JSON escaping | `PR` |
| `TF-NSC-023` | `CF-NSC-003` | repeticion y determinismo: Compilation-log JSON escaping | `PR` |
| `TF-NSC-024` | `CF-NSC-003` | hostil y concurrencia: Compilation-log JSON escaping | `NIGHTLY` |
| `TF-NSC-025` | `CF-NSC-004` | nominal minimo: Atomic payload publication | `PR` |
| `TF-NSC-026` | `CF-NSC-004` | nominal representativo: Atomic payload publication | `PR` |
| `TF-NSC-027` | `CF-NSC-004` | prerrequisito ausente: Atomic payload publication | `PR` |
| `TF-NSC-028` | `CF-NSC-004` | entrada invalida: Atomic payload publication | `PR` |
| `TF-NSC-029` | `CF-NSC-004` | borde inferior: Atomic payload publication | `PR` |
| `TF-NSC-030` | `CF-NSC-004` | borde superior: Atomic payload publication | `PR` |
| `TF-NSC-031` | `CF-NSC-004` | repeticion y determinismo: Atomic payload publication | `PR` |
| `TF-NSC-032` | `CF-NSC-004` | hostil y concurrencia: Atomic payload publication | `NIGHTLY` |
| `TF-NSC-033` | `CF-NSC-005` | nominal minimo: Text and JSON compilation-log schemas | `PR` |
| `TF-NSC-034` | `CF-NSC-005` | nominal representativo: Text and JSON compilation-log schemas | `PR` |
| `TF-NSC-035` | `CF-NSC-005` | prerrequisito ausente: Text and JSON compilation-log schemas | `PR` |
| `TF-NSC-036` | `CF-NSC-005` | entrada invalida: Text and JSON compilation-log schemas | `PR` |
| `TF-NSC-037` | `CF-NSC-005` | borde inferior: Text and JSON compilation-log schemas | `PR` |
| `TF-NSC-038` | `CF-NSC-005` | borde superior: Text and JSON compilation-log schemas | `PR` |
| `TF-NSC-039` | `CF-NSC-005` | repeticion y determinismo: Text and JSON compilation-log schemas | `PR` |
| `TF-NSC-040` | `CF-NSC-005` | hostil y concurrencia: Text and JSON compilation-log schemas | `NIGHTLY` |
| `TF-NSC-041` | `CF-NSC-006` | nominal minimo: Collision-safe automatic error-log allocation | `PR` |
| `TF-NSC-042` | `CF-NSC-006` | nominal representativo: Collision-safe automatic error-log allocation | `PR` |
| `TF-NSC-043` | `CF-NSC-006` | prerrequisito ausente: Collision-safe automatic error-log allocation | `PR` |
| `TF-NSC-044` | `CF-NSC-006` | entrada invalida: Collision-safe automatic error-log allocation | `PR` |
| `TF-NSC-045` | `CF-NSC-006` | borde inferior: Collision-safe automatic error-log allocation | `PR` |
| `TF-NSC-046` | `CF-NSC-006` | borde superior: Collision-safe automatic error-log allocation | `PR` |
| `TF-NSC-047` | `CF-NSC-006` | repeticion y determinismo: Collision-safe automatic error-log allocation | `PR` |
| `TF-NSC-048` | `CF-NSC-006` | hostil y concurrencia: Collision-safe automatic error-log allocation | `NIGHTLY` |
| `TF-NSC-049` | `CF-NSC-007` | nominal minimo: Compilation invocation classification | `PR` |
| `TF-NSC-050` | `CF-NSC-007` | nominal representativo: Compilation invocation classification | `PR` |
| `TF-NSC-051` | `CF-NSC-007` | prerrequisito ausente: Compilation invocation classification | `PR` |
| `TF-NSC-052` | `CF-NSC-007` | entrada invalida: Compilation invocation classification | `PR` |
| `TF-NSC-053` | `CF-NSC-007` | borde inferior: Compilation invocation classification | `PR` |
| `TF-NSC-054` | `CF-NSC-007` | borde superior: Compilation invocation classification | `PR` |
| `TF-NSC-055` | `CF-NSC-007` | repeticion y determinismo: Compilation invocation classification | `PR` |
| `TF-NSC-056` | `CF-NSC-007` | hostil y concurrencia: Compilation invocation classification | `NIGHTLY` |
| `TF-NSC-057` | `CF-NSC-008` | nominal minimo: JSON-output detection and banner suppression | `PR` |
| `TF-NSC-058` | `CF-NSC-008` | nominal representativo: JSON-output detection and banner suppression | `PR` |
| `TF-NSC-059` | `CF-NSC-008` | prerrequisito ausente: JSON-output detection and banner suppression | `PR` |
| `TF-NSC-060` | `CF-NSC-008` | entrada invalida: JSON-output detection and banner suppression | `PR` |
| `TF-NSC-061` | `CF-NSC-008` | borde inferior: JSON-output detection and banner suppression | `PR` |
| `TF-NSC-062` | `CF-NSC-008` | borde superior: JSON-output detection and banner suppression | `PR` |
| `TF-NSC-063` | `CF-NSC-008` | repeticion y determinismo: JSON-output detection and banner suppression | `PR` |
| `TF-NSC-064` | `CF-NSC-008` | hostil y concurrencia: JSON-output detection and banner suppression | `NIGHTLY` |
| `TF-NSC-065` | `CF-NSC-009` | nominal minimo: Project and library-root option extraction | `PR` |
| `TF-NSC-066` | `CF-NSC-009` | nominal representativo: Project and library-root option extraction | `PR` |
| `TF-NSC-067` | `CF-NSC-009` | prerrequisito ausente: Project and library-root option extraction | `PR` |
| `TF-NSC-068` | `CF-NSC-009` | entrada invalida: Project and library-root option extraction | `PR` |
| `TF-NSC-069` | `CF-NSC-009` | borde inferior: Project and library-root option extraction | `PR` |
| `TF-NSC-070` | `CF-NSC-009` | borde superior: Project and library-root option extraction | `PR` |
| `TF-NSC-071` | `CF-NSC-009` | repeticion y determinismo: Project and library-root option extraction | `PR` |
| `TF-NSC-072` | `CF-NSC-009` | hostil y concurrencia: Project and library-root option extraction | `NIGHTLY` |
| `TF-NSC-073` | `CF-NSC-010` | nominal minimo: Progress option parsing | `PR` |
| `TF-NSC-074` | `CF-NSC-010` | nominal representativo: Progress option parsing | `PR` |
| `TF-NSC-075` | `CF-NSC-010` | prerrequisito ausente: Progress option parsing | `PR` |
| `TF-NSC-076` | `CF-NSC-010` | entrada invalida: Progress option parsing | `PR` |
| `TF-NSC-077` | `CF-NSC-010` | borde inferior: Progress option parsing | `PR` |
| `TF-NSC-078` | `CF-NSC-010` | borde superior: Progress option parsing | `PR` |
| `TF-NSC-079` | `CF-NSC-010` | repeticion y determinismo: Progress option parsing | `PR` |
| `TF-NSC-080` | `CF-NSC-010` | hostil y concurrencia: Progress option parsing | `NIGHTLY` |
| `TF-NSC-081` | `CF-NSC-011` | nominal minimo: Global console normalization and language scope | `PR` |
| `TF-NSC-082` | `CF-NSC-011` | nominal representativo: Global console normalization and language scope | `PR` |
| `TF-NSC-083` | `CF-NSC-011` | prerrequisito ausente: Global console normalization and language scope | `PR` |
| `TF-NSC-084` | `CF-NSC-011` | entrada invalida: Global console normalization and language scope | `PR` |
| `TF-NSC-085` | `CF-NSC-011` | borde inferior: Global console normalization and language scope | `PR` |
| `TF-NSC-086` | `CF-NSC-011` | borde superior: Global console normalization and language scope | `PR` |
| `TF-NSC-087` | `CF-NSC-011` | repeticion y determinismo: Global console normalization and language scope | `PR` |
| `TF-NSC-088` | `CF-NSC-011` | hostil y concurrencia: Global console normalization and language scope | `NIGHTLY` |
| `TF-NSC-089` | `CF-NSC-012` | nominal minimo: Verbose build progress and time normalization | `PR` |
| `TF-NSC-090` | `CF-NSC-012` | nominal representativo: Verbose build progress and time normalization | `PR` |
| `TF-NSC-091` | `CF-NSC-012` | prerrequisito ausente: Verbose build progress and time normalization | `PR` |
| `TF-NSC-092` | `CF-NSC-012` | entrada invalida: Verbose build progress and time normalization | `PR` |
| `TF-NSC-093` | `CF-NSC-012` | borde inferior: Verbose build progress and time normalization | `PR` |
| `TF-NSC-094` | `CF-NSC-012` | borde superior: Verbose build progress and time normalization | `PR` |
| `TF-NSC-095` | `CF-NSC-012` | repeticion y determinismo: Verbose build progress and time normalization | `PR` |
| `TF-NSC-096` | `CF-NSC-012` | hostil y concurrencia: Verbose build progress and time normalization | `NIGHTLY` |
| `TF-NSC-097` | `CF-NSC-013` | nominal minimo: Log-format validation | `PR` |
| `TF-NSC-098` | `CF-NSC-013` | nominal representativo: Log-format validation | `PR` |
| `TF-NSC-099` | `CF-NSC-013` | prerrequisito ausente: Log-format validation | `PR` |
| `TF-NSC-100` | `CF-NSC-013` | entrada invalida: Log-format validation | `PR` |
| `TF-NSC-101` | `CF-NSC-013` | borde inferior: Log-format validation | `PR` |
| `TF-NSC-102` | `CF-NSC-013` | borde superior: Log-format validation | `PR` |
| `TF-NSC-103` | `CF-NSC-013` | repeticion y determinismo: Log-format validation | `PR` |
| `TF-NSC-104` | `CF-NSC-013` | hostil y concurrencia: Log-format validation | `NIGHTLY` |
| `TF-NSC-105` | `CF-NSC-014` | nominal minimo: LLVM trace supervisor and worker mode | `PR` |
| `TF-NSC-106` | `CF-NSC-014` | nominal representativo: LLVM trace supervisor and worker mode | `PR` |
| `TF-NSC-107` | `CF-NSC-014` | prerrequisito ausente: LLVM trace supervisor and worker mode | `PR` |
| `TF-NSC-108` | `CF-NSC-014` | entrada invalida: LLVM trace supervisor and worker mode | `PR` |
| `TF-NSC-109` | `CF-NSC-014` | borde inferior: LLVM trace supervisor and worker mode | `PR` |
| `TF-NSC-110` | `CF-NSC-014` | borde superior: LLVM trace supervisor and worker mode | `PR` |
| `TF-NSC-111` | `CF-NSC-014` | repeticion y determinismo: LLVM trace supervisor and worker mode | `PR` |
| `TF-NSC-112` | `CF-NSC-014` | hostil y concurrencia: LLVM trace supervisor and worker mode | `NIGHTLY` |
| `TF-NSC-113` | `CF-NSC-015` | nominal minimo: Captured dispatch banner and top-level progress | `PR` |
| `TF-NSC-114` | `CF-NSC-015` | nominal representativo: Captured dispatch banner and top-level progress | `PR` |
| `TF-NSC-115` | `CF-NSC-015` | prerrequisito ausente: Captured dispatch banner and top-level progress | `PR` |
| `TF-NSC-116` | `CF-NSC-015` | entrada invalida: Captured dispatch banner and top-level progress | `PR` |
| `TF-NSC-117` | `CF-NSC-015` | borde inferior: Captured dispatch banner and top-level progress | `PR` |
| `TF-NSC-118` | `CF-NSC-015` | borde superior: Captured dispatch banner and top-level progress | `PR` |
| `TF-NSC-119` | `CF-NSC-015` | repeticion y determinismo: Captured dispatch banner and top-level progress | `PR` |
| `TF-NSC-120` | `CF-NSC-015` | hostil y concurrencia: Captured dispatch banner and top-level progress | `NIGHTLY` |
| `TF-NSC-121` | `CF-NSC-016` | nominal minimo: Explicit and automatic compilation logs | `PR` |
| `TF-NSC-122` | `CF-NSC-016` | nominal representativo: Explicit and automatic compilation logs | `PR` |
| `TF-NSC-123` | `CF-NSC-016` | prerrequisito ausente: Explicit and automatic compilation logs | `PR` |
| `TF-NSC-124` | `CF-NSC-016` | entrada invalida: Explicit and automatic compilation logs | `PR` |
| `TF-NSC-125` | `CF-NSC-016` | borde inferior: Explicit and automatic compilation logs | `PR` |
| `TF-NSC-126` | `CF-NSC-016` | borde superior: Explicit and automatic compilation logs | `PR` |
| `TF-NSC-127` | `CF-NSC-016` | repeticion y determinismo: Explicit and automatic compilation logs | `PR` |
| `TF-NSC-128` | `CF-NSC-016` | hostil y concurrencia: Explicit and automatic compilation logs | `NIGHTLY` |
| `TF-NSC-129` | `CF-NSC-017` | nominal minimo: Compilation time-trace publication | `PR` |
| `TF-NSC-130` | `CF-NSC-017` | nominal representativo: Compilation time-trace publication | `PR` |
| `TF-NSC-131` | `CF-NSC-017` | prerrequisito ausente: Compilation time-trace publication | `PR` |
| `TF-NSC-132` | `CF-NSC-017` | entrada invalida: Compilation time-trace publication | `PR` |
| `TF-NSC-133` | `CF-NSC-017` | borde inferior: Compilation time-trace publication | `PR` |
| `TF-NSC-134` | `CF-NSC-017` | borde superior: Compilation time-trace publication | `PR` |
| `TF-NSC-135` | `CF-NSC-017` | repeticion y determinismo: Compilation time-trace publication | `PR` |
| `TF-NSC-136` | `CF-NSC-017` | hostil y concurrencia: Compilation time-trace publication | `NIGHTLY` |
| `TF-NSC-137` | `CF-NSC-018` | nominal minimo: Failure diagnostic bundle publication | `PR` |
| `TF-NSC-138` | `CF-NSC-018` | nominal representativo: Failure diagnostic bundle publication | `PR` |
| `TF-NSC-139` | `CF-NSC-018` | prerrequisito ausente: Failure diagnostic bundle publication | `PR` |
| `TF-NSC-140` | `CF-NSC-018` | entrada invalida: Failure diagnostic bundle publication | `PR` |
| `TF-NSC-141` | `CF-NSC-018` | borde inferior: Failure diagnostic bundle publication | `PR` |
| `TF-NSC-142` | `CF-NSC-018` | borde superior: Failure diagnostic bundle publication | `PR` |
| `TF-NSC-143` | `CF-NSC-018` | repeticion y determinismo: Failure diagnostic bundle publication | `PR` |
| `TF-NSC-144` | `CF-NSC-018` | hostil y concurrencia: Failure diagnostic bundle publication | `NIGHTLY` |
| `TF-NSC-145` | `CF-NSC-019` | nominal minimo: Windows UTF-16 argument conversion | `PR` |
| `TF-NSC-146` | `CF-NSC-019` | nominal representativo: Windows UTF-16 argument conversion | `PR` |
| `TF-NSC-147` | `CF-NSC-019` | prerrequisito ausente: Windows UTF-16 argument conversion | `PR` |
| `TF-NSC-148` | `CF-NSC-019` | entrada invalida: Windows UTF-16 argument conversion | `PR` |
| `TF-NSC-149` | `CF-NSC-019` | borde inferior: Windows UTF-16 argument conversion | `PR` |
| `TF-NSC-150` | `CF-NSC-019` | borde superior: Windows UTF-16 argument conversion | `PR` |
| `TF-NSC-151` | `CF-NSC-019` | repeticion y determinismo: Windows UTF-16 argument conversion | `PR` |
| `TF-NSC-152` | `CF-NSC-019` | hostil y concurrencia: Windows UTF-16 argument conversion | `NIGHTLY` |
| `TF-NSC-153` | `CF-NSC-020` | nominal minimo: Platform entry point crash boundary | `PR` |
| `TF-NSC-154` | `CF-NSC-020` | nominal representativo: Platform entry point crash boundary | `PR` |
| `TF-NSC-155` | `CF-NSC-020` | prerrequisito ausente: Platform entry point crash boundary | `PR` |
| `TF-NSC-156` | `CF-NSC-020` | entrada invalida: Platform entry point crash boundary | `PR` |
| `TF-NSC-157` | `CF-NSC-020` | borde inferior: Platform entry point crash boundary | `PR` |
| `TF-NSC-158` | `CF-NSC-020` | borde superior: Platform entry point crash boundary | `PR` |
| `TF-NSC-159` | `CF-NSC-020` | repeticion y determinismo: Platform entry point crash boundary | `PR` |
| `TF-NSC-160` | `CF-NSC-020` | hostil y concurrencia: Platform entry point crash boundary | `NIGHTLY` |

## 5. Gates de implementación futura

- Los 20 contratos conservan ocho casos implementados cada uno.
- Process fixtures aíslan entorno, cwd, filesystem, child process y streams.
- Fallos de observabilidad no pueden reemplazar el exit code del compilador.
- El worker LLVM interno nunca se filtra al Driver como argumento de usuario.
- `wmain` y `main` demuestran la misma frontera de crash con conversión sólo en Windows.
