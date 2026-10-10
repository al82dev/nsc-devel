# Tanda 06-02 — Host.LanguageService — materialización e integración

## SCOPE LOCK operativo

- **Objetivo autorizado:** materializar e integrar exclusivamente los 42 contratos y 336 casos canónicos de
  `Host.LanguageService` definidos por la Fase 1.
- **Naturaleza:** infraestructura de pruebas y documentación; no cambia producto.
- **Revisión canónica:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Comportamiento preservado:** los cuatro archivos de producto, la batería histórica y sus oráculos
  permanecen intactos.
- **Archivos autorizados:** módulos nuevos `LanguageService*`, registro y entrada host, lista de fuentes,
  manifiesto consolidado, gate aislado y este Markdown.
- **Archivos protegidos:** `TRACKING.md`, `CURSOR.json`, producto, batería histórica y los seis cambios
  preexistentes de Fase 1.
- **Acciones prohibidas:** build, pruebas, gates, validación, baseline, recursión, acceso a `C:`, remotos,
  despliegue, modificación de producto y fallback mediante artefactos viejos.
- **Criterio de terminación:** 336 IDs registrados una sola vez; cada ID trazado a su contrato; sólo los 255
  ejecutables ordinarios publicados en el manifiesto consolidado; pendientes y defecto conocido con disposición
  explícita; controlador registrado y evidencia Markdown actualizada; validación declarada pendiente.

## Denominador canónico

| Dimensión | Conteo |
|---|---:|
| Contratos | 42 |
| Casos totales | 336 |
| Casos por contrato | 8 |
| PR / NIGHTLY | 294 / 42 |
| Unit / component | 312 / 24 |
| Ejecutables ordinarios | 255 |
| `InfrastructurePending` | 80 |
| `KnownProductDefect` | 1 |
| Omitidos / desconocidos | 0 / 0 |

Los 255 casos ordinarios conservan 224 casos `PR` y 31 `NIGHTLY`; 231 son unitarios y 24 son de
componente. El manifiesto consolidado no contiene los 80 pendientes ni el defecto conocido y por ello no puede
convertirlos en `PASS`.

## Trazabilidad contrato–oráculo

| Contrato | Superficie | Oráculo | Disposición materializada |
|---|---|---|---|
| `CF-LANGUAGE-SERVICE-001` | Position/range values | exact equality and containment | `Required` |
| `CF-LANGUAGE-SERVICE-002` | Diagnostic transport | field-by-field payload | `Required` |
| `CF-LANGUAGE-SERVICE-003` | Symbol/location transport | field-by-field payload | `Required` |
| `CF-LANGUAGE-SERVICE-004` | Completion/signature transport | field-by-field payload | `Required` |
| `CF-LANGUAGE-SERVICE-005` | Edit/action/rename/token transport | field-by-field payload | `Required` |
| `CF-LANGUAGE-SERVICE-006` | UTF-8 scalar decoder | decoded scalar and UTF-16 width | `InfrastructurePending` |
| `CF-LANGUAGE-SERVICE-007` | Byte offset to UTF-16 | exact position | `Required` |
| `CF-LANGUAGE-SERVICE-008` | UTF-16 to byte offset | exact offset | `Required` |
| `CF-LANGUAGE-SERVICE-009` | Compiler diagnostic conversion | exact converted payload | `Required` |
| `CF-LANGUAGE-SERVICE-010` | Service identity and ABI | exact identity fields | `Required` |
| `CF-LANGUAGE-SERVICE-011` | Canonical path keys | canonical path and equivalent lookup | `Required` |
| `CF-LANGUAGE-SERVICE-012` | Snapshot neutral accessors | exact accessor values | `Required` |
| `CF-LANGUAGE-SERVICE-013` | Document open | boolean, error, stored snapshot | `Required` |
| `CF-LANGUAGE-SERVICE-014` | Document update | boolean, error, snapshot | `Required` |
| `CF-LANGUAGE-SERVICE-015` | Document close/query/count | result and count | `Required` |
| `CF-LANGUAGE-SERVICE-016` | Brace shielding | observable formatter indentation | `Required` |
| `CF-LANGUAGE-SERVICE-017` | Formatter policy | exact formatted text | `Required` |
| `CF-LANGUAGE-SERVICE-018` | Inline assembly shield | byte-identical formatting no-op | `Required` |
| `CF-LANGUAGE-SERVICE-019` | Whole-document format edit | range and replacement text | `Required` |
| `CF-LANGUAGE-SERVICE-020` | Code actions | ordered action payloads | `Required` |
| `CF-LANGUAGE-SERVICE-021` | Semantic-index ownership | index sizes and retained owner | `InfrastructurePending` |
| `CF-LANGUAGE-SERVICE-022` | Source/identifier locations | path and ranges | `InfrastructurePending` |
| `CF-LANGUAGE-SERVICE-023` | Frontend kind mapping | exact kind table | `InfrastructurePending` |
| `CF-LANGUAGE-SERVICE-024` | Callable detail rendering | exact label | `InfrastructurePending` |
| `CF-LANGUAGE-SERVICE-025` | Semantic symbol population | symbol set and details | `InfrastructurePending` |
| `CF-LANGUAGE-SERVICE-026` | Semantic reference indexing | targets and exactness | `InfrastructurePending` |
| `CF-LANGUAGE-SERVICE-027` | Call/signature indexing | call payload | `InfrastructurePending` |
| `CF-LANGUAGE-SERVICE-028` | Smallest matching range | selected identity | `InfrastructurePending` |
| `CF-LANGUAGE-SERVICE-029` | Target grouping/overloads | exact unique ordinals | `InfrastructurePending` |
| `CF-LANGUAGE-SERVICE-030` | Document symbols | ordered symbol list | `Required` |
| `CF-LANGUAGE-SERVICE-031` | Workspace symbols | ordered capped list | `Required` |
| `CF-LANGUAGE-SERVICE-032` | Symbol at position | selected symbol | `Required` |
| `CF-LANGUAGE-SERVICE-033` | Definitions | ordered locations | `Required` |
| `CF-LANGUAGE-SERVICE-034` | Completions | labels, details, kinds, order | `Required` |
| `CF-LANGUAGE-SERVICE-035` | Signature help | signature and active indices | `Required` |
| `CF-LANGUAGE-SERVICE-036` | References | exact unique locations | `Required` |
| `CF-LANGUAGE-SERVICE-037` | Rename preparation | range or rejection | `Required` |
| `CF-LANGUAGE-SERVICE-038` | Atomic rename | allowed, error, exact edit set | `7 Required + 1 KnownProductDefect` |
| `CF-LANGUAGE-SERVICE-039` | Semantic tokens | ranges, kinds, flags, order | `Required` |
| `CF-LANGUAGE-SERVICE-040` | Analysis request | generation, project, cancellation, success | `Required` |
| `CF-LANGUAGE-SERVICE-041` | Overlay analysis | compiled source choice | `Required` |
| `CF-LANGUAGE-SERVICE-042` | Diagnostics/fallback publication | flags, diagnostics, index identity | `Required` |

Cada fila representa ocho IDs consecutivos. La única división interna es
`CF-LANGUAGE-SERVICE-038`: `TF-LANGUAGE-SERVICE-297..303` son ordinarios y
`TF-LANGUAGE-SERVICE-304` conserva `KnownProductDefect`.

## Infraestructura pendiente explícita

- `CF-LANGUAGE-SERVICE-006`: `decodeUtf8` y `utf16Width` viven en namespace anónimo. Las conversiones
  públicas no sustituyen el oráculo directo de scalar y width.
- `CF-LANGUAGE-SERVICE-021..029`: construcción, almacenamiento y selección de `SemanticIndex` dependen de
  helpers privados y modelos frontend internos. Probarlos como unidad exige un seam controlado todavía no
  autorizado.

Los 80 casos están presentes en el registro mediante `TestDisposition::InfrastructurePending`. Su función
falla deliberadamente si alguien intenta ejecutarla fuera de la selección ordinaria; nunca retorna éxito
vacío.

## Defecto de producto conocido

`TF-LANGUAGE-SERVICE-304` materializa el caso hostil de atomic rename como
`TestDisposition::KnownProductDefect`. Corresponde a `CON-0010`: `validRenameIdentifier` mantiene una tabla
privada de keywords que diverge de lexer/parser para el alias léxico `i32`. El oráculo exige rechazo; el producto
actual permite el rename y por tanto el caso falla de forma reproducible. El caso no se publica en el manifiesto ejecutable, no se
marca como `SKIP` ni se presenta como `PASS`; permanece seleccionable directamente por ID para exponer el
defecto. Corregir producto queda fuera del alcance.

## Módulos y responsabilidad

- `LanguageServiceProtocol001To080Module.cpp`: DTOs, UTF-16, diagnósticos e identidad.
- `LanguageServiceDocuments081To160Module.cpp`: paths, snapshots, documentos, formato y acciones.
- `LanguageServiceIndex161To232Module.cpp`: pendientes explícitos del índice privado.
- `LanguageServiceQueries233To312Module.cpp`: consultas semánticas, rename y tokens con proyecto temporal.
- `LanguageServiceAnalysis313To336Module.cpp`: generaciones, overlays, cancelación, diagnósticos y fallback.
- `LanguageServiceTestSupport.h`: IDs, disposiciones, árbol temporal y fixture semántico compartido.
- `manifest/cases.jsonl`: 255 filas ejecutables ordinarias fusionadas, sin duplicados.
- `run_host_language_service_gate.py`: filtra el manifiesto central y verifica denominador ejecutable,
  perfiles, capas, IDs y resultado.

El flujo es iterativo. No se incorporó recursión. Los proyectos de consulta se crean en un directorio temporal
por caso y el fixture conserva el mismo contrato observable de Stage 27 sin depender de la batería histórica.

## Integración central

El registro compartido y `main.cpp` declaran y llaman estos cinco símbolos:

- `registerLanguageServiceProtocol001To080Module`;
- `registerLanguageServiceDocuments081To160Module`;
- `registerLanguageServiceIndex161To232Module`;
- `registerLanguageServiceQueries233To312Module`;
- `registerLanguageServiceAnalysis313To336Module`.

`CMakeLists.txt` incorpora los cinco módulos, las fuentes estrictamente necesarias
`src/LanguageService/ProtocolTypes.cpp` y `src/LanguageService/LanguageService.cpp`, y registra el controlador
aislado. Las 255 filas ejecutables se fusionaron en `manifest/cases.jsonl`; una inspección declarativa posterior
confirmó 255 IDs únicos de componente, 32 contratos ejecutables, cero duplicados del componente y cero
duplicados globales. Después de esa confirmación se retiró el fragmento temporal.

## Estado de validación

No se ejecutaron build, pruebas, gates, runners ni validación dinámica, por orden expresa de Fase 2. Sólo se
realizaron inspecciones declarativas de conteo, distribución y unicidad sobre los archivos integrados.
La conclusión exacta es:

`IMPLEMENTACIÓN E INTEGRACIÓN ESTÁTICA COMPLETAS; VALIDACIÓN DINÁMICA PENDIENTE`.

