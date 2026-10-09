# Plan de pruebas — Host.LanguageService

## 1. Alcance

Plan contractual diseñado desde los cuatro archivos auditados. No implementa ni ejecuta pruebas.

## 2. Fixtures futuros

- Textos UTF-8 con ASCII, CR, LF, CRLF, BMP, pares sustitutos e inválidos.
- Proyectos mínimos con overlays, símbolos, overloads, referencias y llamadas resueltas.
- Snapshots válidos, fallidos, cancelados y con último modelo semántico válido.
- Documentos con comentarios, literales, llaves, asm y rangos de edición.

## 3. Contratos

| ID | Contrato | Capa | Oráculo |
|---|---|---|---|
| `CF-LANGUAGE-SERVICE-001` | Position and range value semantics | `unit` | exact equality and containment |
| `CF-LANGUAGE-SERVICE-002` | Diagnostic transport schema | `unit` | field-by-field payload |
| `CF-LANGUAGE-SERVICE-003` | Symbol and location transport schema | `unit` | field-by-field payload |
| `CF-LANGUAGE-SERVICE-004` | Completion and signature transport schema | `unit` | field-by-field payload |
| `CF-LANGUAGE-SERVICE-005` | Edit action rename and semantic-token schemas | `unit` | field-by-field payload |
| `CF-LANGUAGE-SERVICE-006` | UTF-8 scalar decoding and width | `unit` | decoded scalar and width |
| `CF-LANGUAGE-SERVICE-007` | Byte offset to UTF-16 position | `unit` | exact position |
| `CF-LANGUAGE-SERVICE-008` | UTF-16 position to byte offset | `unit` | exact offset |
| `CF-LANGUAGE-SERVICE-009` | Compiler diagnostic to language diagnostic | `unit` | exact converted payload |
| `CF-LANGUAGE-SERVICE-010` | Language-service identity and analysis ABI | `unit` | exact identity fields |
| `CF-LANGUAGE-SERVICE-011` | Canonical case-insensitive path keys | `unit` | key and canonical path |
| `CF-LANGUAGE-SERVICE-012` | Snapshot lifetime and neutral accessors | `unit` | exact accessor values |
| `CF-LANGUAGE-SERVICE-013` | Document open admission | `unit` | boolean, error text and stored snapshot |
| `CF-LANGUAGE-SERVICE-014` | Document update monotonicity | `unit` | boolean, error text and snapshot |
| `CF-LANGUAGE-SERVICE-015` | Document close query and count | `unit` | result and count |
| `CF-LANGUAGE-SERVICE-016` | Brace scanner lexical shielding | `unit` | opens closes and beginsWithClose |
| `CF-LANGUAGE-SERVICE-017` | Formatter indentation newline and option policy | `unit` | exact formatted text |
| `CF-LANGUAGE-SERVICE-018` | Inline assembly formatting shield | `unit` | boolean and byte-identical no-op |
| `CF-LANGUAGE-SERVICE-019` | Whole-document formatting edit | `unit` | edit range and replacement text |
| `CF-LANGUAGE-SERVICE-020` | Language code actions | `unit` | ordered action payloads |
| `CF-LANGUAGE-SERVICE-021` | Semantic index ownership and storage | `unit` | index sizes and retained owner |
| `CF-LANGUAGE-SERVICE-022` | Source and identifier locations | `unit` | path and ranges |
| `CF-LANGUAGE-SERVICE-023` | Frontend kind to language-symbol kind mapping | `unit` | exact kind table |
| `CF-LANGUAGE-SERVICE-024` | Callable detail rendering | `unit` | exact label |
| `CF-LANGUAGE-SERVICE-025` | Semantic symbol population | `unit` | symbol set and details |
| `CF-LANGUAGE-SERVICE-026` | Semantic reference indexing | `unit` | reference targets and exactness |
| `CF-LANGUAGE-SERVICE-027` | Call and signature indexing | `unit` | call payload |
| `CF-LANGUAGE-SERVICE-028` | Smallest matching range selection | `unit` | selected identity |
| `CF-LANGUAGE-SERVICE-029` | Target grouping and overload expansion | `unit` | exact unique ordinals |
| `CF-LANGUAGE-SERVICE-030` | Document symbol query | `unit` | ordered symbol list |
| `CF-LANGUAGE-SERVICE-031` | Workspace symbol query | `unit` | ordered capped list |
| `CF-LANGUAGE-SERVICE-032` | Symbol-at-position query | `unit` | selected symbol |
| `CF-LANGUAGE-SERVICE-033` | Definition query | `unit` | ordered locations |
| `CF-LANGUAGE-SERVICE-034` | Completion query | `unit` | labels details kinds and order |
| `CF-LANGUAGE-SERVICE-035` | Signature help query | `unit` | signature and active indices |
| `CF-LANGUAGE-SERVICE-036` | Reference query | `unit` | exact unique locations |
| `CF-LANGUAGE-SERVICE-037` | Rename eligibility and preparation | `unit` | range or rejection text |
| `CF-LANGUAGE-SERVICE-038` | Atomic rename edit set | `unit` | allowed flag, error and edits |
| `CF-LANGUAGE-SERVICE-039` | Semantic token projection | `unit` | token ranges kinds flags and order |
| `CF-LANGUAGE-SERVICE-040` | Analysis request generation jobs and cancellation | `component` | generation, project, cancellation and success |
| `CF-LANGUAGE-SERVICE-041` | Open-document overlay analysis | `component` | compiled source choice |
| `CF-LANGUAGE-SERVICE-042` | Diagnostics publication and last-good semantic fallback | `component` | flags, diagnostics and index identity |

## 4. Casos diseñados

| ID | Contrato | Objetivo | Frecuencia |
|---|---|---|---|
| `TF-LANGUAGE-SERVICE-001` | `CF-LANGUAGE-SERVICE-001` | nominal minimo: Position and range value semantics | `PR` |
| `TF-LANGUAGE-SERVICE-002` | `CF-LANGUAGE-SERVICE-001` | nominal representativo: Position and range value semantics | `PR` |
| `TF-LANGUAGE-SERVICE-003` | `CF-LANGUAGE-SERVICE-001` | prerrequisito ausente: Position and range value semantics | `PR` |
| `TF-LANGUAGE-SERVICE-004` | `CF-LANGUAGE-SERVICE-001` | entrada invalida: Position and range value semantics | `PR` |
| `TF-LANGUAGE-SERVICE-005` | `CF-LANGUAGE-SERVICE-001` | borde inferior: Position and range value semantics | `PR` |
| `TF-LANGUAGE-SERVICE-006` | `CF-LANGUAGE-SERVICE-001` | borde superior: Position and range value semantics | `PR` |
| `TF-LANGUAGE-SERVICE-007` | `CF-LANGUAGE-SERVICE-001` | repeticion y determinismo: Position and range value semantics | `PR` |
| `TF-LANGUAGE-SERVICE-008` | `CF-LANGUAGE-SERVICE-001` | hostil y estres: Position and range value semantics | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-009` | `CF-LANGUAGE-SERVICE-002` | nominal minimo: Diagnostic transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-010` | `CF-LANGUAGE-SERVICE-002` | nominal representativo: Diagnostic transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-011` | `CF-LANGUAGE-SERVICE-002` | prerrequisito ausente: Diagnostic transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-012` | `CF-LANGUAGE-SERVICE-002` | entrada invalida: Diagnostic transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-013` | `CF-LANGUAGE-SERVICE-002` | borde inferior: Diagnostic transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-014` | `CF-LANGUAGE-SERVICE-002` | borde superior: Diagnostic transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-015` | `CF-LANGUAGE-SERVICE-002` | repeticion y determinismo: Diagnostic transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-016` | `CF-LANGUAGE-SERVICE-002` | hostil y estres: Diagnostic transport schema | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-017` | `CF-LANGUAGE-SERVICE-003` | nominal minimo: Symbol and location transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-018` | `CF-LANGUAGE-SERVICE-003` | nominal representativo: Symbol and location transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-019` | `CF-LANGUAGE-SERVICE-003` | prerrequisito ausente: Symbol and location transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-020` | `CF-LANGUAGE-SERVICE-003` | entrada invalida: Symbol and location transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-021` | `CF-LANGUAGE-SERVICE-003` | borde inferior: Symbol and location transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-022` | `CF-LANGUAGE-SERVICE-003` | borde superior: Symbol and location transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-023` | `CF-LANGUAGE-SERVICE-003` | repeticion y determinismo: Symbol and location transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-024` | `CF-LANGUAGE-SERVICE-003` | hostil y estres: Symbol and location transport schema | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-025` | `CF-LANGUAGE-SERVICE-004` | nominal minimo: Completion and signature transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-026` | `CF-LANGUAGE-SERVICE-004` | nominal representativo: Completion and signature transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-027` | `CF-LANGUAGE-SERVICE-004` | prerrequisito ausente: Completion and signature transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-028` | `CF-LANGUAGE-SERVICE-004` | entrada invalida: Completion and signature transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-029` | `CF-LANGUAGE-SERVICE-004` | borde inferior: Completion and signature transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-030` | `CF-LANGUAGE-SERVICE-004` | borde superior: Completion and signature transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-031` | `CF-LANGUAGE-SERVICE-004` | repeticion y determinismo: Completion and signature transport schema | `PR` |
| `TF-LANGUAGE-SERVICE-032` | `CF-LANGUAGE-SERVICE-004` | hostil y estres: Completion and signature transport schema | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-033` | `CF-LANGUAGE-SERVICE-005` | nominal minimo: Edit action rename and semantic-token schemas | `PR` |
| `TF-LANGUAGE-SERVICE-034` | `CF-LANGUAGE-SERVICE-005` | nominal representativo: Edit action rename and semantic-token schemas | `PR` |
| `TF-LANGUAGE-SERVICE-035` | `CF-LANGUAGE-SERVICE-005` | prerrequisito ausente: Edit action rename and semantic-token schemas | `PR` |
| `TF-LANGUAGE-SERVICE-036` | `CF-LANGUAGE-SERVICE-005` | entrada invalida: Edit action rename and semantic-token schemas | `PR` |
| `TF-LANGUAGE-SERVICE-037` | `CF-LANGUAGE-SERVICE-005` | borde inferior: Edit action rename and semantic-token schemas | `PR` |
| `TF-LANGUAGE-SERVICE-038` | `CF-LANGUAGE-SERVICE-005` | borde superior: Edit action rename and semantic-token schemas | `PR` |
| `TF-LANGUAGE-SERVICE-039` | `CF-LANGUAGE-SERVICE-005` | repeticion y determinismo: Edit action rename and semantic-token schemas | `PR` |
| `TF-LANGUAGE-SERVICE-040` | `CF-LANGUAGE-SERVICE-005` | hostil y estres: Edit action rename and semantic-token schemas | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-041` | `CF-LANGUAGE-SERVICE-006` | nominal minimo: UTF-8 scalar decoding and width | `PR` |
| `TF-LANGUAGE-SERVICE-042` | `CF-LANGUAGE-SERVICE-006` | nominal representativo: UTF-8 scalar decoding and width | `PR` |
| `TF-LANGUAGE-SERVICE-043` | `CF-LANGUAGE-SERVICE-006` | prerrequisito ausente: UTF-8 scalar decoding and width | `PR` |
| `TF-LANGUAGE-SERVICE-044` | `CF-LANGUAGE-SERVICE-006` | entrada invalida: UTF-8 scalar decoding and width | `PR` |
| `TF-LANGUAGE-SERVICE-045` | `CF-LANGUAGE-SERVICE-006` | borde inferior: UTF-8 scalar decoding and width | `PR` |
| `TF-LANGUAGE-SERVICE-046` | `CF-LANGUAGE-SERVICE-006` | borde superior: UTF-8 scalar decoding and width | `PR` |
| `TF-LANGUAGE-SERVICE-047` | `CF-LANGUAGE-SERVICE-006` | repeticion y determinismo: UTF-8 scalar decoding and width | `PR` |
| `TF-LANGUAGE-SERVICE-048` | `CF-LANGUAGE-SERVICE-006` | hostil y estres: UTF-8 scalar decoding and width | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-049` | `CF-LANGUAGE-SERVICE-007` | nominal minimo: Byte offset to UTF-16 position | `PR` |
| `TF-LANGUAGE-SERVICE-050` | `CF-LANGUAGE-SERVICE-007` | nominal representativo: Byte offset to UTF-16 position | `PR` |
| `TF-LANGUAGE-SERVICE-051` | `CF-LANGUAGE-SERVICE-007` | prerrequisito ausente: Byte offset to UTF-16 position | `PR` |
| `TF-LANGUAGE-SERVICE-052` | `CF-LANGUAGE-SERVICE-007` | entrada invalida: Byte offset to UTF-16 position | `PR` |
| `TF-LANGUAGE-SERVICE-053` | `CF-LANGUAGE-SERVICE-007` | borde inferior: Byte offset to UTF-16 position | `PR` |
| `TF-LANGUAGE-SERVICE-054` | `CF-LANGUAGE-SERVICE-007` | borde superior: Byte offset to UTF-16 position | `PR` |
| `TF-LANGUAGE-SERVICE-055` | `CF-LANGUAGE-SERVICE-007` | repeticion y determinismo: Byte offset to UTF-16 position | `PR` |
| `TF-LANGUAGE-SERVICE-056` | `CF-LANGUAGE-SERVICE-007` | hostil y estres: Byte offset to UTF-16 position | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-057` | `CF-LANGUAGE-SERVICE-008` | nominal minimo: UTF-16 position to byte offset | `PR` |
| `TF-LANGUAGE-SERVICE-058` | `CF-LANGUAGE-SERVICE-008` | nominal representativo: UTF-16 position to byte offset | `PR` |
| `TF-LANGUAGE-SERVICE-059` | `CF-LANGUAGE-SERVICE-008` | prerrequisito ausente: UTF-16 position to byte offset | `PR` |
| `TF-LANGUAGE-SERVICE-060` | `CF-LANGUAGE-SERVICE-008` | entrada invalida: UTF-16 position to byte offset | `PR` |
| `TF-LANGUAGE-SERVICE-061` | `CF-LANGUAGE-SERVICE-008` | borde inferior: UTF-16 position to byte offset | `PR` |
| `TF-LANGUAGE-SERVICE-062` | `CF-LANGUAGE-SERVICE-008` | borde superior: UTF-16 position to byte offset | `PR` |
| `TF-LANGUAGE-SERVICE-063` | `CF-LANGUAGE-SERVICE-008` | repeticion y determinismo: UTF-16 position to byte offset | `PR` |
| `TF-LANGUAGE-SERVICE-064` | `CF-LANGUAGE-SERVICE-008` | hostil y estres: UTF-16 position to byte offset | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-065` | `CF-LANGUAGE-SERVICE-009` | nominal minimo: Compiler diagnostic to language diagnostic | `PR` |
| `TF-LANGUAGE-SERVICE-066` | `CF-LANGUAGE-SERVICE-009` | nominal representativo: Compiler diagnostic to language diagnostic | `PR` |
| `TF-LANGUAGE-SERVICE-067` | `CF-LANGUAGE-SERVICE-009` | prerrequisito ausente: Compiler diagnostic to language diagnostic | `PR` |
| `TF-LANGUAGE-SERVICE-068` | `CF-LANGUAGE-SERVICE-009` | entrada invalida: Compiler diagnostic to language diagnostic | `PR` |
| `TF-LANGUAGE-SERVICE-069` | `CF-LANGUAGE-SERVICE-009` | borde inferior: Compiler diagnostic to language diagnostic | `PR` |
| `TF-LANGUAGE-SERVICE-070` | `CF-LANGUAGE-SERVICE-009` | borde superior: Compiler diagnostic to language diagnostic | `PR` |
| `TF-LANGUAGE-SERVICE-071` | `CF-LANGUAGE-SERVICE-009` | repeticion y determinismo: Compiler diagnostic to language diagnostic | `PR` |
| `TF-LANGUAGE-SERVICE-072` | `CF-LANGUAGE-SERVICE-009` | hostil y estres: Compiler diagnostic to language diagnostic | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-073` | `CF-LANGUAGE-SERVICE-010` | nominal minimo: Language-service identity and analysis ABI | `PR` |
| `TF-LANGUAGE-SERVICE-074` | `CF-LANGUAGE-SERVICE-010` | nominal representativo: Language-service identity and analysis ABI | `PR` |
| `TF-LANGUAGE-SERVICE-075` | `CF-LANGUAGE-SERVICE-010` | prerrequisito ausente: Language-service identity and analysis ABI | `PR` |
| `TF-LANGUAGE-SERVICE-076` | `CF-LANGUAGE-SERVICE-010` | entrada invalida: Language-service identity and analysis ABI | `PR` |
| `TF-LANGUAGE-SERVICE-077` | `CF-LANGUAGE-SERVICE-010` | borde inferior: Language-service identity and analysis ABI | `PR` |
| `TF-LANGUAGE-SERVICE-078` | `CF-LANGUAGE-SERVICE-010` | borde superior: Language-service identity and analysis ABI | `PR` |
| `TF-LANGUAGE-SERVICE-079` | `CF-LANGUAGE-SERVICE-010` | repeticion y determinismo: Language-service identity and analysis ABI | `PR` |
| `TF-LANGUAGE-SERVICE-080` | `CF-LANGUAGE-SERVICE-010` | hostil y estres: Language-service identity and analysis ABI | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-081` | `CF-LANGUAGE-SERVICE-011` | nominal minimo: Canonical case-insensitive path keys | `PR` |
| `TF-LANGUAGE-SERVICE-082` | `CF-LANGUAGE-SERVICE-011` | nominal representativo: Canonical case-insensitive path keys | `PR` |
| `TF-LANGUAGE-SERVICE-083` | `CF-LANGUAGE-SERVICE-011` | prerrequisito ausente: Canonical case-insensitive path keys | `PR` |
| `TF-LANGUAGE-SERVICE-084` | `CF-LANGUAGE-SERVICE-011` | entrada invalida: Canonical case-insensitive path keys | `PR` |
| `TF-LANGUAGE-SERVICE-085` | `CF-LANGUAGE-SERVICE-011` | borde inferior: Canonical case-insensitive path keys | `PR` |
| `TF-LANGUAGE-SERVICE-086` | `CF-LANGUAGE-SERVICE-011` | borde superior: Canonical case-insensitive path keys | `PR` |
| `TF-LANGUAGE-SERVICE-087` | `CF-LANGUAGE-SERVICE-011` | repeticion y determinismo: Canonical case-insensitive path keys | `PR` |
| `TF-LANGUAGE-SERVICE-088` | `CF-LANGUAGE-SERVICE-011` | hostil y estres: Canonical case-insensitive path keys | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-089` | `CF-LANGUAGE-SERVICE-012` | nominal minimo: Snapshot lifetime and neutral accessors | `PR` |
| `TF-LANGUAGE-SERVICE-090` | `CF-LANGUAGE-SERVICE-012` | nominal representativo: Snapshot lifetime and neutral accessors | `PR` |
| `TF-LANGUAGE-SERVICE-091` | `CF-LANGUAGE-SERVICE-012` | prerrequisito ausente: Snapshot lifetime and neutral accessors | `PR` |
| `TF-LANGUAGE-SERVICE-092` | `CF-LANGUAGE-SERVICE-012` | entrada invalida: Snapshot lifetime and neutral accessors | `PR` |
| `TF-LANGUAGE-SERVICE-093` | `CF-LANGUAGE-SERVICE-012` | borde inferior: Snapshot lifetime and neutral accessors | `PR` |
| `TF-LANGUAGE-SERVICE-094` | `CF-LANGUAGE-SERVICE-012` | borde superior: Snapshot lifetime and neutral accessors | `PR` |
| `TF-LANGUAGE-SERVICE-095` | `CF-LANGUAGE-SERVICE-012` | repeticion y determinismo: Snapshot lifetime and neutral accessors | `PR` |
| `TF-LANGUAGE-SERVICE-096` | `CF-LANGUAGE-SERVICE-012` | hostil y estres: Snapshot lifetime and neutral accessors | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-097` | `CF-LANGUAGE-SERVICE-013` | nominal minimo: Document open admission | `PR` |
| `TF-LANGUAGE-SERVICE-098` | `CF-LANGUAGE-SERVICE-013` | nominal representativo: Document open admission | `PR` |
| `TF-LANGUAGE-SERVICE-099` | `CF-LANGUAGE-SERVICE-013` | prerrequisito ausente: Document open admission | `PR` |
| `TF-LANGUAGE-SERVICE-100` | `CF-LANGUAGE-SERVICE-013` | entrada invalida: Document open admission | `PR` |
| `TF-LANGUAGE-SERVICE-101` | `CF-LANGUAGE-SERVICE-013` | borde inferior: Document open admission | `PR` |
| `TF-LANGUAGE-SERVICE-102` | `CF-LANGUAGE-SERVICE-013` | borde superior: Document open admission | `PR` |
| `TF-LANGUAGE-SERVICE-103` | `CF-LANGUAGE-SERVICE-013` | repeticion y determinismo: Document open admission | `PR` |
| `TF-LANGUAGE-SERVICE-104` | `CF-LANGUAGE-SERVICE-013` | hostil y estres: Document open admission | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-105` | `CF-LANGUAGE-SERVICE-014` | nominal minimo: Document update monotonicity | `PR` |
| `TF-LANGUAGE-SERVICE-106` | `CF-LANGUAGE-SERVICE-014` | nominal representativo: Document update monotonicity | `PR` |
| `TF-LANGUAGE-SERVICE-107` | `CF-LANGUAGE-SERVICE-014` | prerrequisito ausente: Document update monotonicity | `PR` |
| `TF-LANGUAGE-SERVICE-108` | `CF-LANGUAGE-SERVICE-014` | entrada invalida: Document update monotonicity | `PR` |
| `TF-LANGUAGE-SERVICE-109` | `CF-LANGUAGE-SERVICE-014` | borde inferior: Document update monotonicity | `PR` |
| `TF-LANGUAGE-SERVICE-110` | `CF-LANGUAGE-SERVICE-014` | borde superior: Document update monotonicity | `PR` |
| `TF-LANGUAGE-SERVICE-111` | `CF-LANGUAGE-SERVICE-014` | repeticion y determinismo: Document update monotonicity | `PR` |
| `TF-LANGUAGE-SERVICE-112` | `CF-LANGUAGE-SERVICE-014` | hostil y estres: Document update monotonicity | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-113` | `CF-LANGUAGE-SERVICE-015` | nominal minimo: Document close query and count | `PR` |
| `TF-LANGUAGE-SERVICE-114` | `CF-LANGUAGE-SERVICE-015` | nominal representativo: Document close query and count | `PR` |
| `TF-LANGUAGE-SERVICE-115` | `CF-LANGUAGE-SERVICE-015` | prerrequisito ausente: Document close query and count | `PR` |
| `TF-LANGUAGE-SERVICE-116` | `CF-LANGUAGE-SERVICE-015` | entrada invalida: Document close query and count | `PR` |
| `TF-LANGUAGE-SERVICE-117` | `CF-LANGUAGE-SERVICE-015` | borde inferior: Document close query and count | `PR` |
| `TF-LANGUAGE-SERVICE-118` | `CF-LANGUAGE-SERVICE-015` | borde superior: Document close query and count | `PR` |
| `TF-LANGUAGE-SERVICE-119` | `CF-LANGUAGE-SERVICE-015` | repeticion y determinismo: Document close query and count | `PR` |
| `TF-LANGUAGE-SERVICE-120` | `CF-LANGUAGE-SERVICE-015` | hostil y estres: Document close query and count | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-121` | `CF-LANGUAGE-SERVICE-016` | nominal minimo: Brace scanner lexical shielding | `PR` |
| `TF-LANGUAGE-SERVICE-122` | `CF-LANGUAGE-SERVICE-016` | nominal representativo: Brace scanner lexical shielding | `PR` |
| `TF-LANGUAGE-SERVICE-123` | `CF-LANGUAGE-SERVICE-016` | prerrequisito ausente: Brace scanner lexical shielding | `PR` |
| `TF-LANGUAGE-SERVICE-124` | `CF-LANGUAGE-SERVICE-016` | entrada invalida: Brace scanner lexical shielding | `PR` |
| `TF-LANGUAGE-SERVICE-125` | `CF-LANGUAGE-SERVICE-016` | borde inferior: Brace scanner lexical shielding | `PR` |
| `TF-LANGUAGE-SERVICE-126` | `CF-LANGUAGE-SERVICE-016` | borde superior: Brace scanner lexical shielding | `PR` |
| `TF-LANGUAGE-SERVICE-127` | `CF-LANGUAGE-SERVICE-016` | repeticion y determinismo: Brace scanner lexical shielding | `PR` |
| `TF-LANGUAGE-SERVICE-128` | `CF-LANGUAGE-SERVICE-016` | hostil y estres: Brace scanner lexical shielding | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-129` | `CF-LANGUAGE-SERVICE-017` | nominal minimo: Formatter indentation newline and option policy | `PR` |
| `TF-LANGUAGE-SERVICE-130` | `CF-LANGUAGE-SERVICE-017` | nominal representativo: Formatter indentation newline and option policy | `PR` |
| `TF-LANGUAGE-SERVICE-131` | `CF-LANGUAGE-SERVICE-017` | prerrequisito ausente: Formatter indentation newline and option policy | `PR` |
| `TF-LANGUAGE-SERVICE-132` | `CF-LANGUAGE-SERVICE-017` | entrada invalida: Formatter indentation newline and option policy | `PR` |
| `TF-LANGUAGE-SERVICE-133` | `CF-LANGUAGE-SERVICE-017` | borde inferior: Formatter indentation newline and option policy | `PR` |
| `TF-LANGUAGE-SERVICE-134` | `CF-LANGUAGE-SERVICE-017` | borde superior: Formatter indentation newline and option policy | `PR` |
| `TF-LANGUAGE-SERVICE-135` | `CF-LANGUAGE-SERVICE-017` | repeticion y determinismo: Formatter indentation newline and option policy | `PR` |
| `TF-LANGUAGE-SERVICE-136` | `CF-LANGUAGE-SERVICE-017` | hostil y estres: Formatter indentation newline and option policy | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-137` | `CF-LANGUAGE-SERVICE-018` | nominal minimo: Inline assembly formatting shield | `PR` |
| `TF-LANGUAGE-SERVICE-138` | `CF-LANGUAGE-SERVICE-018` | nominal representativo: Inline assembly formatting shield | `PR` |
| `TF-LANGUAGE-SERVICE-139` | `CF-LANGUAGE-SERVICE-018` | prerrequisito ausente: Inline assembly formatting shield | `PR` |
| `TF-LANGUAGE-SERVICE-140` | `CF-LANGUAGE-SERVICE-018` | entrada invalida: Inline assembly formatting shield | `PR` |
| `TF-LANGUAGE-SERVICE-141` | `CF-LANGUAGE-SERVICE-018` | borde inferior: Inline assembly formatting shield | `PR` |
| `TF-LANGUAGE-SERVICE-142` | `CF-LANGUAGE-SERVICE-018` | borde superior: Inline assembly formatting shield | `PR` |
| `TF-LANGUAGE-SERVICE-143` | `CF-LANGUAGE-SERVICE-018` | repeticion y determinismo: Inline assembly formatting shield | `PR` |
| `TF-LANGUAGE-SERVICE-144` | `CF-LANGUAGE-SERVICE-018` | hostil y estres: Inline assembly formatting shield | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-145` | `CF-LANGUAGE-SERVICE-019` | nominal minimo: Whole-document formatting edit | `PR` |
| `TF-LANGUAGE-SERVICE-146` | `CF-LANGUAGE-SERVICE-019` | nominal representativo: Whole-document formatting edit | `PR` |
| `TF-LANGUAGE-SERVICE-147` | `CF-LANGUAGE-SERVICE-019` | prerrequisito ausente: Whole-document formatting edit | `PR` |
| `TF-LANGUAGE-SERVICE-148` | `CF-LANGUAGE-SERVICE-019` | entrada invalida: Whole-document formatting edit | `PR` |
| `TF-LANGUAGE-SERVICE-149` | `CF-LANGUAGE-SERVICE-019` | borde inferior: Whole-document formatting edit | `PR` |
| `TF-LANGUAGE-SERVICE-150` | `CF-LANGUAGE-SERVICE-019` | borde superior: Whole-document formatting edit | `PR` |
| `TF-LANGUAGE-SERVICE-151` | `CF-LANGUAGE-SERVICE-019` | repeticion y determinismo: Whole-document formatting edit | `PR` |
| `TF-LANGUAGE-SERVICE-152` | `CF-LANGUAGE-SERVICE-019` | hostil y estres: Whole-document formatting edit | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-153` | `CF-LANGUAGE-SERVICE-020` | nominal minimo: Language code actions | `PR` |
| `TF-LANGUAGE-SERVICE-154` | `CF-LANGUAGE-SERVICE-020` | nominal representativo: Language code actions | `PR` |
| `TF-LANGUAGE-SERVICE-155` | `CF-LANGUAGE-SERVICE-020` | prerrequisito ausente: Language code actions | `PR` |
| `TF-LANGUAGE-SERVICE-156` | `CF-LANGUAGE-SERVICE-020` | entrada invalida: Language code actions | `PR` |
| `TF-LANGUAGE-SERVICE-157` | `CF-LANGUAGE-SERVICE-020` | borde inferior: Language code actions | `PR` |
| `TF-LANGUAGE-SERVICE-158` | `CF-LANGUAGE-SERVICE-020` | borde superior: Language code actions | `PR` |
| `TF-LANGUAGE-SERVICE-159` | `CF-LANGUAGE-SERVICE-020` | repeticion y determinismo: Language code actions | `PR` |
| `TF-LANGUAGE-SERVICE-160` | `CF-LANGUAGE-SERVICE-020` | hostil y estres: Language code actions | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-161` | `CF-LANGUAGE-SERVICE-021` | nominal minimo: Semantic index ownership and storage | `PR` |
| `TF-LANGUAGE-SERVICE-162` | `CF-LANGUAGE-SERVICE-021` | nominal representativo: Semantic index ownership and storage | `PR` |
| `TF-LANGUAGE-SERVICE-163` | `CF-LANGUAGE-SERVICE-021` | prerrequisito ausente: Semantic index ownership and storage | `PR` |
| `TF-LANGUAGE-SERVICE-164` | `CF-LANGUAGE-SERVICE-021` | entrada invalida: Semantic index ownership and storage | `PR` |
| `TF-LANGUAGE-SERVICE-165` | `CF-LANGUAGE-SERVICE-021` | borde inferior: Semantic index ownership and storage | `PR` |
| `TF-LANGUAGE-SERVICE-166` | `CF-LANGUAGE-SERVICE-021` | borde superior: Semantic index ownership and storage | `PR` |
| `TF-LANGUAGE-SERVICE-167` | `CF-LANGUAGE-SERVICE-021` | repeticion y determinismo: Semantic index ownership and storage | `PR` |
| `TF-LANGUAGE-SERVICE-168` | `CF-LANGUAGE-SERVICE-021` | hostil y estres: Semantic index ownership and storage | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-169` | `CF-LANGUAGE-SERVICE-022` | nominal minimo: Source and identifier locations | `PR` |
| `TF-LANGUAGE-SERVICE-170` | `CF-LANGUAGE-SERVICE-022` | nominal representativo: Source and identifier locations | `PR` |
| `TF-LANGUAGE-SERVICE-171` | `CF-LANGUAGE-SERVICE-022` | prerrequisito ausente: Source and identifier locations | `PR` |
| `TF-LANGUAGE-SERVICE-172` | `CF-LANGUAGE-SERVICE-022` | entrada invalida: Source and identifier locations | `PR` |
| `TF-LANGUAGE-SERVICE-173` | `CF-LANGUAGE-SERVICE-022` | borde inferior: Source and identifier locations | `PR` |
| `TF-LANGUAGE-SERVICE-174` | `CF-LANGUAGE-SERVICE-022` | borde superior: Source and identifier locations | `PR` |
| `TF-LANGUAGE-SERVICE-175` | `CF-LANGUAGE-SERVICE-022` | repeticion y determinismo: Source and identifier locations | `PR` |
| `TF-LANGUAGE-SERVICE-176` | `CF-LANGUAGE-SERVICE-022` | hostil y estres: Source and identifier locations | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-177` | `CF-LANGUAGE-SERVICE-023` | nominal minimo: Frontend kind to language-symbol kind mapping | `PR` |
| `TF-LANGUAGE-SERVICE-178` | `CF-LANGUAGE-SERVICE-023` | nominal representativo: Frontend kind to language-symbol kind mapping | `PR` |
| `TF-LANGUAGE-SERVICE-179` | `CF-LANGUAGE-SERVICE-023` | prerrequisito ausente: Frontend kind to language-symbol kind mapping | `PR` |
| `TF-LANGUAGE-SERVICE-180` | `CF-LANGUAGE-SERVICE-023` | entrada invalida: Frontend kind to language-symbol kind mapping | `PR` |
| `TF-LANGUAGE-SERVICE-181` | `CF-LANGUAGE-SERVICE-023` | borde inferior: Frontend kind to language-symbol kind mapping | `PR` |
| `TF-LANGUAGE-SERVICE-182` | `CF-LANGUAGE-SERVICE-023` | borde superior: Frontend kind to language-symbol kind mapping | `PR` |
| `TF-LANGUAGE-SERVICE-183` | `CF-LANGUAGE-SERVICE-023` | repeticion y determinismo: Frontend kind to language-symbol kind mapping | `PR` |
| `TF-LANGUAGE-SERVICE-184` | `CF-LANGUAGE-SERVICE-023` | hostil y estres: Frontend kind to language-symbol kind mapping | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-185` | `CF-LANGUAGE-SERVICE-024` | nominal minimo: Callable detail rendering | `PR` |
| `TF-LANGUAGE-SERVICE-186` | `CF-LANGUAGE-SERVICE-024` | nominal representativo: Callable detail rendering | `PR` |
| `TF-LANGUAGE-SERVICE-187` | `CF-LANGUAGE-SERVICE-024` | prerrequisito ausente: Callable detail rendering | `PR` |
| `TF-LANGUAGE-SERVICE-188` | `CF-LANGUAGE-SERVICE-024` | entrada invalida: Callable detail rendering | `PR` |
| `TF-LANGUAGE-SERVICE-189` | `CF-LANGUAGE-SERVICE-024` | borde inferior: Callable detail rendering | `PR` |
| `TF-LANGUAGE-SERVICE-190` | `CF-LANGUAGE-SERVICE-024` | borde superior: Callable detail rendering | `PR` |
| `TF-LANGUAGE-SERVICE-191` | `CF-LANGUAGE-SERVICE-024` | repeticion y determinismo: Callable detail rendering | `PR` |
| `TF-LANGUAGE-SERVICE-192` | `CF-LANGUAGE-SERVICE-024` | hostil y estres: Callable detail rendering | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-193` | `CF-LANGUAGE-SERVICE-025` | nominal minimo: Semantic symbol population | `PR` |
| `TF-LANGUAGE-SERVICE-194` | `CF-LANGUAGE-SERVICE-025` | nominal representativo: Semantic symbol population | `PR` |
| `TF-LANGUAGE-SERVICE-195` | `CF-LANGUAGE-SERVICE-025` | prerrequisito ausente: Semantic symbol population | `PR` |
| `TF-LANGUAGE-SERVICE-196` | `CF-LANGUAGE-SERVICE-025` | entrada invalida: Semantic symbol population | `PR` |
| `TF-LANGUAGE-SERVICE-197` | `CF-LANGUAGE-SERVICE-025` | borde inferior: Semantic symbol population | `PR` |
| `TF-LANGUAGE-SERVICE-198` | `CF-LANGUAGE-SERVICE-025` | borde superior: Semantic symbol population | `PR` |
| `TF-LANGUAGE-SERVICE-199` | `CF-LANGUAGE-SERVICE-025` | repeticion y determinismo: Semantic symbol population | `PR` |
| `TF-LANGUAGE-SERVICE-200` | `CF-LANGUAGE-SERVICE-025` | hostil y estres: Semantic symbol population | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-201` | `CF-LANGUAGE-SERVICE-026` | nominal minimo: Semantic reference indexing | `PR` |
| `TF-LANGUAGE-SERVICE-202` | `CF-LANGUAGE-SERVICE-026` | nominal representativo: Semantic reference indexing | `PR` |
| `TF-LANGUAGE-SERVICE-203` | `CF-LANGUAGE-SERVICE-026` | prerrequisito ausente: Semantic reference indexing | `PR` |
| `TF-LANGUAGE-SERVICE-204` | `CF-LANGUAGE-SERVICE-026` | entrada invalida: Semantic reference indexing | `PR` |
| `TF-LANGUAGE-SERVICE-205` | `CF-LANGUAGE-SERVICE-026` | borde inferior: Semantic reference indexing | `PR` |
| `TF-LANGUAGE-SERVICE-206` | `CF-LANGUAGE-SERVICE-026` | borde superior: Semantic reference indexing | `PR` |
| `TF-LANGUAGE-SERVICE-207` | `CF-LANGUAGE-SERVICE-026` | repeticion y determinismo: Semantic reference indexing | `PR` |
| `TF-LANGUAGE-SERVICE-208` | `CF-LANGUAGE-SERVICE-026` | hostil y estres: Semantic reference indexing | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-209` | `CF-LANGUAGE-SERVICE-027` | nominal minimo: Call and signature indexing | `PR` |
| `TF-LANGUAGE-SERVICE-210` | `CF-LANGUAGE-SERVICE-027` | nominal representativo: Call and signature indexing | `PR` |
| `TF-LANGUAGE-SERVICE-211` | `CF-LANGUAGE-SERVICE-027` | prerrequisito ausente: Call and signature indexing | `PR` |
| `TF-LANGUAGE-SERVICE-212` | `CF-LANGUAGE-SERVICE-027` | entrada invalida: Call and signature indexing | `PR` |
| `TF-LANGUAGE-SERVICE-213` | `CF-LANGUAGE-SERVICE-027` | borde inferior: Call and signature indexing | `PR` |
| `TF-LANGUAGE-SERVICE-214` | `CF-LANGUAGE-SERVICE-027` | borde superior: Call and signature indexing | `PR` |
| `TF-LANGUAGE-SERVICE-215` | `CF-LANGUAGE-SERVICE-027` | repeticion y determinismo: Call and signature indexing | `PR` |
| `TF-LANGUAGE-SERVICE-216` | `CF-LANGUAGE-SERVICE-027` | hostil y estres: Call and signature indexing | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-217` | `CF-LANGUAGE-SERVICE-028` | nominal minimo: Smallest matching range selection | `PR` |
| `TF-LANGUAGE-SERVICE-218` | `CF-LANGUAGE-SERVICE-028` | nominal representativo: Smallest matching range selection | `PR` |
| `TF-LANGUAGE-SERVICE-219` | `CF-LANGUAGE-SERVICE-028` | prerrequisito ausente: Smallest matching range selection | `PR` |
| `TF-LANGUAGE-SERVICE-220` | `CF-LANGUAGE-SERVICE-028` | entrada invalida: Smallest matching range selection | `PR` |
| `TF-LANGUAGE-SERVICE-221` | `CF-LANGUAGE-SERVICE-028` | borde inferior: Smallest matching range selection | `PR` |
| `TF-LANGUAGE-SERVICE-222` | `CF-LANGUAGE-SERVICE-028` | borde superior: Smallest matching range selection | `PR` |
| `TF-LANGUAGE-SERVICE-223` | `CF-LANGUAGE-SERVICE-028` | repeticion y determinismo: Smallest matching range selection | `PR` |
| `TF-LANGUAGE-SERVICE-224` | `CF-LANGUAGE-SERVICE-028` | hostil y estres: Smallest matching range selection | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-225` | `CF-LANGUAGE-SERVICE-029` | nominal minimo: Target grouping and overload expansion | `PR` |
| `TF-LANGUAGE-SERVICE-226` | `CF-LANGUAGE-SERVICE-029` | nominal representativo: Target grouping and overload expansion | `PR` |
| `TF-LANGUAGE-SERVICE-227` | `CF-LANGUAGE-SERVICE-029` | prerrequisito ausente: Target grouping and overload expansion | `PR` |
| `TF-LANGUAGE-SERVICE-228` | `CF-LANGUAGE-SERVICE-029` | entrada invalida: Target grouping and overload expansion | `PR` |
| `TF-LANGUAGE-SERVICE-229` | `CF-LANGUAGE-SERVICE-029` | borde inferior: Target grouping and overload expansion | `PR` |
| `TF-LANGUAGE-SERVICE-230` | `CF-LANGUAGE-SERVICE-029` | borde superior: Target grouping and overload expansion | `PR` |
| `TF-LANGUAGE-SERVICE-231` | `CF-LANGUAGE-SERVICE-029` | repeticion y determinismo: Target grouping and overload expansion | `PR` |
| `TF-LANGUAGE-SERVICE-232` | `CF-LANGUAGE-SERVICE-029` | hostil y estres: Target grouping and overload expansion | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-233` | `CF-LANGUAGE-SERVICE-030` | nominal minimo: Document symbol query | `PR` |
| `TF-LANGUAGE-SERVICE-234` | `CF-LANGUAGE-SERVICE-030` | nominal representativo: Document symbol query | `PR` |
| `TF-LANGUAGE-SERVICE-235` | `CF-LANGUAGE-SERVICE-030` | prerrequisito ausente: Document symbol query | `PR` |
| `TF-LANGUAGE-SERVICE-236` | `CF-LANGUAGE-SERVICE-030` | entrada invalida: Document symbol query | `PR` |
| `TF-LANGUAGE-SERVICE-237` | `CF-LANGUAGE-SERVICE-030` | borde inferior: Document symbol query | `PR` |
| `TF-LANGUAGE-SERVICE-238` | `CF-LANGUAGE-SERVICE-030` | borde superior: Document symbol query | `PR` |
| `TF-LANGUAGE-SERVICE-239` | `CF-LANGUAGE-SERVICE-030` | repeticion y determinismo: Document symbol query | `PR` |
| `TF-LANGUAGE-SERVICE-240` | `CF-LANGUAGE-SERVICE-030` | hostil y estres: Document symbol query | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-241` | `CF-LANGUAGE-SERVICE-031` | nominal minimo: Workspace symbol query | `PR` |
| `TF-LANGUAGE-SERVICE-242` | `CF-LANGUAGE-SERVICE-031` | nominal representativo: Workspace symbol query | `PR` |
| `TF-LANGUAGE-SERVICE-243` | `CF-LANGUAGE-SERVICE-031` | prerrequisito ausente: Workspace symbol query | `PR` |
| `TF-LANGUAGE-SERVICE-244` | `CF-LANGUAGE-SERVICE-031` | entrada invalida: Workspace symbol query | `PR` |
| `TF-LANGUAGE-SERVICE-245` | `CF-LANGUAGE-SERVICE-031` | borde inferior: Workspace symbol query | `PR` |
| `TF-LANGUAGE-SERVICE-246` | `CF-LANGUAGE-SERVICE-031` | borde superior: Workspace symbol query | `PR` |
| `TF-LANGUAGE-SERVICE-247` | `CF-LANGUAGE-SERVICE-031` | repeticion y determinismo: Workspace symbol query | `PR` |
| `TF-LANGUAGE-SERVICE-248` | `CF-LANGUAGE-SERVICE-031` | hostil y estres: Workspace symbol query | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-249` | `CF-LANGUAGE-SERVICE-032` | nominal minimo: Symbol-at-position query | `PR` |
| `TF-LANGUAGE-SERVICE-250` | `CF-LANGUAGE-SERVICE-032` | nominal representativo: Symbol-at-position query | `PR` |
| `TF-LANGUAGE-SERVICE-251` | `CF-LANGUAGE-SERVICE-032` | prerrequisito ausente: Symbol-at-position query | `PR` |
| `TF-LANGUAGE-SERVICE-252` | `CF-LANGUAGE-SERVICE-032` | entrada invalida: Symbol-at-position query | `PR` |
| `TF-LANGUAGE-SERVICE-253` | `CF-LANGUAGE-SERVICE-032` | borde inferior: Symbol-at-position query | `PR` |
| `TF-LANGUAGE-SERVICE-254` | `CF-LANGUAGE-SERVICE-032` | borde superior: Symbol-at-position query | `PR` |
| `TF-LANGUAGE-SERVICE-255` | `CF-LANGUAGE-SERVICE-032` | repeticion y determinismo: Symbol-at-position query | `PR` |
| `TF-LANGUAGE-SERVICE-256` | `CF-LANGUAGE-SERVICE-032` | hostil y estres: Symbol-at-position query | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-257` | `CF-LANGUAGE-SERVICE-033` | nominal minimo: Definition query | `PR` |
| `TF-LANGUAGE-SERVICE-258` | `CF-LANGUAGE-SERVICE-033` | nominal representativo: Definition query | `PR` |
| `TF-LANGUAGE-SERVICE-259` | `CF-LANGUAGE-SERVICE-033` | prerrequisito ausente: Definition query | `PR` |
| `TF-LANGUAGE-SERVICE-260` | `CF-LANGUAGE-SERVICE-033` | entrada invalida: Definition query | `PR` |
| `TF-LANGUAGE-SERVICE-261` | `CF-LANGUAGE-SERVICE-033` | borde inferior: Definition query | `PR` |
| `TF-LANGUAGE-SERVICE-262` | `CF-LANGUAGE-SERVICE-033` | borde superior: Definition query | `PR` |
| `TF-LANGUAGE-SERVICE-263` | `CF-LANGUAGE-SERVICE-033` | repeticion y determinismo: Definition query | `PR` |
| `TF-LANGUAGE-SERVICE-264` | `CF-LANGUAGE-SERVICE-033` | hostil y estres: Definition query | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-265` | `CF-LANGUAGE-SERVICE-034` | nominal minimo: Completion query | `PR` |
| `TF-LANGUAGE-SERVICE-266` | `CF-LANGUAGE-SERVICE-034` | nominal representativo: Completion query | `PR` |
| `TF-LANGUAGE-SERVICE-267` | `CF-LANGUAGE-SERVICE-034` | prerrequisito ausente: Completion query | `PR` |
| `TF-LANGUAGE-SERVICE-268` | `CF-LANGUAGE-SERVICE-034` | entrada invalida: Completion query | `PR` |
| `TF-LANGUAGE-SERVICE-269` | `CF-LANGUAGE-SERVICE-034` | borde inferior: Completion query | `PR` |
| `TF-LANGUAGE-SERVICE-270` | `CF-LANGUAGE-SERVICE-034` | borde superior: Completion query | `PR` |
| `TF-LANGUAGE-SERVICE-271` | `CF-LANGUAGE-SERVICE-034` | repeticion y determinismo: Completion query | `PR` |
| `TF-LANGUAGE-SERVICE-272` | `CF-LANGUAGE-SERVICE-034` | hostil y estres: Completion query | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-273` | `CF-LANGUAGE-SERVICE-035` | nominal minimo: Signature help query | `PR` |
| `TF-LANGUAGE-SERVICE-274` | `CF-LANGUAGE-SERVICE-035` | nominal representativo: Signature help query | `PR` |
| `TF-LANGUAGE-SERVICE-275` | `CF-LANGUAGE-SERVICE-035` | prerrequisito ausente: Signature help query | `PR` |
| `TF-LANGUAGE-SERVICE-276` | `CF-LANGUAGE-SERVICE-035` | entrada invalida: Signature help query | `PR` |
| `TF-LANGUAGE-SERVICE-277` | `CF-LANGUAGE-SERVICE-035` | borde inferior: Signature help query | `PR` |
| `TF-LANGUAGE-SERVICE-278` | `CF-LANGUAGE-SERVICE-035` | borde superior: Signature help query | `PR` |
| `TF-LANGUAGE-SERVICE-279` | `CF-LANGUAGE-SERVICE-035` | repeticion y determinismo: Signature help query | `PR` |
| `TF-LANGUAGE-SERVICE-280` | `CF-LANGUAGE-SERVICE-035` | hostil y estres: Signature help query | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-281` | `CF-LANGUAGE-SERVICE-036` | nominal minimo: Reference query | `PR` |
| `TF-LANGUAGE-SERVICE-282` | `CF-LANGUAGE-SERVICE-036` | nominal representativo: Reference query | `PR` |
| `TF-LANGUAGE-SERVICE-283` | `CF-LANGUAGE-SERVICE-036` | prerrequisito ausente: Reference query | `PR` |
| `TF-LANGUAGE-SERVICE-284` | `CF-LANGUAGE-SERVICE-036` | entrada invalida: Reference query | `PR` |
| `TF-LANGUAGE-SERVICE-285` | `CF-LANGUAGE-SERVICE-036` | borde inferior: Reference query | `PR` |
| `TF-LANGUAGE-SERVICE-286` | `CF-LANGUAGE-SERVICE-036` | borde superior: Reference query | `PR` |
| `TF-LANGUAGE-SERVICE-287` | `CF-LANGUAGE-SERVICE-036` | repeticion y determinismo: Reference query | `PR` |
| `TF-LANGUAGE-SERVICE-288` | `CF-LANGUAGE-SERVICE-036` | hostil y estres: Reference query | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-289` | `CF-LANGUAGE-SERVICE-037` | nominal minimo: Rename eligibility and preparation | `PR` |
| `TF-LANGUAGE-SERVICE-290` | `CF-LANGUAGE-SERVICE-037` | nominal representativo: Rename eligibility and preparation | `PR` |
| `TF-LANGUAGE-SERVICE-291` | `CF-LANGUAGE-SERVICE-037` | prerrequisito ausente: Rename eligibility and preparation | `PR` |
| `TF-LANGUAGE-SERVICE-292` | `CF-LANGUAGE-SERVICE-037` | entrada invalida: Rename eligibility and preparation | `PR` |
| `TF-LANGUAGE-SERVICE-293` | `CF-LANGUAGE-SERVICE-037` | borde inferior: Rename eligibility and preparation | `PR` |
| `TF-LANGUAGE-SERVICE-294` | `CF-LANGUAGE-SERVICE-037` | borde superior: Rename eligibility and preparation | `PR` |
| `TF-LANGUAGE-SERVICE-295` | `CF-LANGUAGE-SERVICE-037` | repeticion y determinismo: Rename eligibility and preparation | `PR` |
| `TF-LANGUAGE-SERVICE-296` | `CF-LANGUAGE-SERVICE-037` | hostil y estres: Rename eligibility and preparation | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-297` | `CF-LANGUAGE-SERVICE-038` | nominal minimo: Atomic rename edit set | `PR` |
| `TF-LANGUAGE-SERVICE-298` | `CF-LANGUAGE-SERVICE-038` | nominal representativo: Atomic rename edit set | `PR` |
| `TF-LANGUAGE-SERVICE-299` | `CF-LANGUAGE-SERVICE-038` | prerrequisito ausente: Atomic rename edit set | `PR` |
| `TF-LANGUAGE-SERVICE-300` | `CF-LANGUAGE-SERVICE-038` | entrada invalida: Atomic rename edit set | `PR` |
| `TF-LANGUAGE-SERVICE-301` | `CF-LANGUAGE-SERVICE-038` | borde inferior: Atomic rename edit set | `PR` |
| `TF-LANGUAGE-SERVICE-302` | `CF-LANGUAGE-SERVICE-038` | borde superior: Atomic rename edit set | `PR` |
| `TF-LANGUAGE-SERVICE-303` | `CF-LANGUAGE-SERVICE-038` | repeticion y determinismo: Atomic rename edit set | `PR` |
| `TF-LANGUAGE-SERVICE-304` | `CF-LANGUAGE-SERVICE-038` | hostil y estres: Atomic rename edit set | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-305` | `CF-LANGUAGE-SERVICE-039` | nominal minimo: Semantic token projection | `PR` |
| `TF-LANGUAGE-SERVICE-306` | `CF-LANGUAGE-SERVICE-039` | nominal representativo: Semantic token projection | `PR` |
| `TF-LANGUAGE-SERVICE-307` | `CF-LANGUAGE-SERVICE-039` | prerrequisito ausente: Semantic token projection | `PR` |
| `TF-LANGUAGE-SERVICE-308` | `CF-LANGUAGE-SERVICE-039` | entrada invalida: Semantic token projection | `PR` |
| `TF-LANGUAGE-SERVICE-309` | `CF-LANGUAGE-SERVICE-039` | borde inferior: Semantic token projection | `PR` |
| `TF-LANGUAGE-SERVICE-310` | `CF-LANGUAGE-SERVICE-039` | borde superior: Semantic token projection | `PR` |
| `TF-LANGUAGE-SERVICE-311` | `CF-LANGUAGE-SERVICE-039` | repeticion y determinismo: Semantic token projection | `PR` |
| `TF-LANGUAGE-SERVICE-312` | `CF-LANGUAGE-SERVICE-039` | hostil y estres: Semantic token projection | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-313` | `CF-LANGUAGE-SERVICE-040` | nominal minimo: Analysis request generation jobs and cancellation | `PR` |
| `TF-LANGUAGE-SERVICE-314` | `CF-LANGUAGE-SERVICE-040` | nominal representativo: Analysis request generation jobs and cancellation | `PR` |
| `TF-LANGUAGE-SERVICE-315` | `CF-LANGUAGE-SERVICE-040` | prerrequisito ausente: Analysis request generation jobs and cancellation | `PR` |
| `TF-LANGUAGE-SERVICE-316` | `CF-LANGUAGE-SERVICE-040` | entrada invalida: Analysis request generation jobs and cancellation | `PR` |
| `TF-LANGUAGE-SERVICE-317` | `CF-LANGUAGE-SERVICE-040` | borde inferior: Analysis request generation jobs and cancellation | `PR` |
| `TF-LANGUAGE-SERVICE-318` | `CF-LANGUAGE-SERVICE-040` | borde superior: Analysis request generation jobs and cancellation | `PR` |
| `TF-LANGUAGE-SERVICE-319` | `CF-LANGUAGE-SERVICE-040` | repeticion y determinismo: Analysis request generation jobs and cancellation | `PR` |
| `TF-LANGUAGE-SERVICE-320` | `CF-LANGUAGE-SERVICE-040` | hostil y estres: Analysis request generation jobs and cancellation | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-321` | `CF-LANGUAGE-SERVICE-041` | nominal minimo: Open-document overlay analysis | `PR` |
| `TF-LANGUAGE-SERVICE-322` | `CF-LANGUAGE-SERVICE-041` | nominal representativo: Open-document overlay analysis | `PR` |
| `TF-LANGUAGE-SERVICE-323` | `CF-LANGUAGE-SERVICE-041` | prerrequisito ausente: Open-document overlay analysis | `PR` |
| `TF-LANGUAGE-SERVICE-324` | `CF-LANGUAGE-SERVICE-041` | entrada invalida: Open-document overlay analysis | `PR` |
| `TF-LANGUAGE-SERVICE-325` | `CF-LANGUAGE-SERVICE-041` | borde inferior: Open-document overlay analysis | `PR` |
| `TF-LANGUAGE-SERVICE-326` | `CF-LANGUAGE-SERVICE-041` | borde superior: Open-document overlay analysis | `PR` |
| `TF-LANGUAGE-SERVICE-327` | `CF-LANGUAGE-SERVICE-041` | repeticion y determinismo: Open-document overlay analysis | `PR` |
| `TF-LANGUAGE-SERVICE-328` | `CF-LANGUAGE-SERVICE-041` | hostil y estres: Open-document overlay analysis | `NIGHTLY` |
| `TF-LANGUAGE-SERVICE-329` | `CF-LANGUAGE-SERVICE-042` | nominal minimo: Diagnostics publication and last-good semantic fallback | `PR` |
| `TF-LANGUAGE-SERVICE-330` | `CF-LANGUAGE-SERVICE-042` | nominal representativo: Diagnostics publication and last-good semantic fallback | `PR` |
| `TF-LANGUAGE-SERVICE-331` | `CF-LANGUAGE-SERVICE-042` | prerrequisito ausente: Diagnostics publication and last-good semantic fallback | `PR` |
| `TF-LANGUAGE-SERVICE-332` | `CF-LANGUAGE-SERVICE-042` | entrada invalida: Diagnostics publication and last-good semantic fallback | `PR` |
| `TF-LANGUAGE-SERVICE-333` | `CF-LANGUAGE-SERVICE-042` | borde inferior: Diagnostics publication and last-good semantic fallback | `PR` |
| `TF-LANGUAGE-SERVICE-334` | `CF-LANGUAGE-SERVICE-042` | borde superior: Diagnostics publication and last-good semantic fallback | `PR` |
| `TF-LANGUAGE-SERVICE-335` | `CF-LANGUAGE-SERVICE-042` | repeticion y determinismo: Diagnostics publication and last-good semantic fallback | `PR` |
| `TF-LANGUAGE-SERVICE-336` | `CF-LANGUAGE-SERVICE-042` | hostil y estres: Diagnostics publication and last-good semantic fallback | `NIGHTLY` |

## 5. Gates de implementación futura

- Los 42 contratos deben conservar ocho casos implementados cada uno.
- UTF-8/UTF-16 debe demostrar inversas sólo en fronteras escalares válidas.
- Consultas sin modelo deben permanecer neutrales y no observar almacenamiento inválido.
- Fallback semántico debe distinguir diagnóstico actual de índice anterior.
- Rename debe fallar sin ediciones parciales ante cualquier referencia inexacta.
- Ningún caso NIGHTLY sustituye los siete casos PR de su contrato.
