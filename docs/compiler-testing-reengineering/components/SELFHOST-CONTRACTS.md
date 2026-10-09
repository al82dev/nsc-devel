# SelfHost.Contracts — disposición y contrato reconstructivo

## 1. Estado y procedencia

- **Revisión:** `65d4dcf44143c7a945a786c8f1e6ba78d225ad02`.
- **Universo:** 48 archivos, 1500 líneas, 48 declaraciones/artefactos.
- **Disposición:** INDIRECTA=48.
- **Diseño futuro:** 48 contratos y 288 casos; ninguna prueba fue creada o ejecutada.

## 2. Propósito y frontera

Declarar oráculos versionados de frontend, semántica, IR, backend, runtime, determinismo y promoción self-host.

**Dependencias:** SelfHost.Bootstrap.Infrastructure;fixtures selfhost;productos SH41.

`DIRECTA` significa comportamiento propio aislable; `INDIRECTA`, comportamiento observable mediante integración o gate; `UPSTREAM`, responsabilidad del proveedor más smoke de integración; `EXCLUIDA`, fuera de la suite NSC con razón persistida.

## 3. Inventario archivo por archivo

| ID | Ruta | Cat. | Disp. | Contrato | Propósito |
|---|---|---|---|---|---|
| PF-000544 | `selfhost/contracts/advanced-toolchain.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-001` | Oráculo self-host advanced toolchain |
| PF-000545 | `selfhost/contracts/arrays-memory.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-002` | Oráculo self-host arrays memory |
| PF-000546 | `selfhost/contracts/backend-general.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-003` | Oráculo self-host backend general |
| PF-000547 | `selfhost/contracts/closure.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-004` | Oráculo self-host closure |
| PF-000548 | `selfhost/contracts/cross-machine-determinism.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-005` | Oráculo self-host cross machine determinism |
| PF-000549 | `selfhost/contracts/data-layout.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-006` | Oráculo self-host data layout |
| PF-000550 | `selfhost/contracts/developer-experience.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-007` | Oráculo self-host developer experience |
| PF-000551 | `selfhost/contracts/diagnostics.map.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-008` | Oráculo self-host diagnostics.map |
| PF-000552 | `selfhost/contracts/distribution-final.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-009` | Oráculo self-host distribution final |
| PF-000553 | `selfhost/contracts/final-certification.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-010` | Oráculo self-host final certification |
| PF-000554 | `selfhost/contracts/first-generation.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-011` | Oráculo self-host first generation |
| PF-000555 | `selfhost/contracts/fixed-point.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-012` | Oráculo self-host fixed point |
| PF-000556 | `selfhost/contracts/frontend.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-013` | Oráculo self-host frontend |
| PF-000557 | `selfhost/contracts/functional-equivalence.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-014` | Oráculo self-host functional equivalence |
| PF-000558 | `selfhost/contracts/generics-aot.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-015` | Oráculo self-host generics aot |
| PF-000559 | `selfhost/contracts/image-replay-retirement.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-016` | Oráculo self-host image replay retirement |
| PF-000560 | `selfhost/contracts/interop-base.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-017` | Oráculo self-host interop base |
| PF-000561 | `selfhost/contracts/interop-parity.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-018` | Oráculo self-host interop parity |
| PF-000562 | `selfhost/contracts/ir.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-019` | Oráculo self-host ir |
| PF-000563 | `selfhost/contracts/llvm-backend.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-020` | Oráculo self-host llvm backend |
| PF-000564 | `selfhost/contracts/llvm-c-api.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-021` | Oráculo self-host llvm c api |
| PF-000565 | `selfhost/contracts/low-level-parity.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-022` | Oráculo self-host low level parity |
| PF-000566 | `selfhost/contracts/module-set.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-023` | Oráculo self-host module set |
| PF-000567 | `selfhost/contracts/native-text.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-024` | Oráculo self-host native text |
| PF-000568 | `selfhost/contracts/object-model.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-025` | Oráculo self-host object model |
| PF-000569 | `selfhost/contracts/performance.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-026` | Oráculo self-host performance |
| PF-000570 | `selfhost/contracts/program-ir.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-027` | Oráculo self-host program ir |
| PF-000571 | `selfhost/contracts/project-driver.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-028` | Oráculo self-host project driver |
| PF-000572 | `selfhost/contracts/promotion-readiness.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-029` | Oráculo self-host promotion readiness |
| PF-000573 | `selfhost/contracts/README.md` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-030` | Oráculo self-host README |
| PF-000574 | `selfhost/contracts/repeated-fixed-point.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-031` | Oráculo self-host repeated fixed point |
| PF-000575 | `selfhost/contracts/robustness-final.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-032` | Oráculo self-host robustness final |
| PF-000576 | `selfhost/contracts/robustness.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-033` | Oráculo self-host robustness |
| PF-000577 | `selfhost/contracts/runtime-bootstrap.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-034` | Oráculo self-host runtime bootstrap |
| PF-000578 | `selfhost/contracts/runtime-library.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-035` | Oráculo self-host runtime library |
| PF-000579 | `selfhost/contracts/self-hosted-generation.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-036` | Oráculo self-host self hosted generation |
| PF-000580 | `selfhost/contracts/semantic.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-037` | Oráculo self-host semantic |
| PF-000581 | `selfhost/contracts/source-bool-void.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-038` | Oráculo self-host source bool void |
| PF-000582 | `selfhost/contracts/source-control-flow.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-039` | Oráculo self-host source control flow |
| PF-000583 | `selfhost/contracts/source-derived-backend.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-040` | Oráculo self-host source derived backend |
| PF-000584 | `selfhost/contracts/source-expressions.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-041` | Oráculo self-host source expressions |
| PF-000585 | `selfhost/contracts/source-floating-scalars.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-042` | Oráculo self-host source floating scalars |
| PF-000586 | `selfhost/contracts/source-functions.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-043` | Oráculo self-host source functions |
| PF-000587 | `selfhost/contracts/source-graph.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-044` | Oráculo self-host source graph |
| PF-000588 | `selfhost/contracts/source-integer-widths.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-045` | Oráculo self-host source integer widths |
| PF-000589 | `selfhost/contracts/source-loop-control.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-046` | Oráculo self-host source loop control |
| PF-000590 | `selfhost/contracts/source-pointers.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-047` | Oráculo self-host source pointers |
| PF-000591 | `selfhost/contracts/specialized-code.contract.json` | `SELFHOST_ORACLE` | `INDIRECTA` | `C12B-SHC-048` | Oráculo self-host specialized code |

## 4. Contratos mantenidos

| Contrato | Nombre | Capa | Entradas | Oráculo |
|---|---|---|---|---|
| `C12B-SHC-001` | Oráculo self-host advanced toolchain | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-002` | Oráculo self-host arrays memory | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-003` | Oráculo self-host backend general | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-004` | Oráculo self-host closure | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-005` | Oráculo self-host cross machine determinism | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-006` | Oráculo self-host data layout | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-007` | Oráculo self-host developer experience | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-008` | Oráculo self-host diagnostics.map | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-009` | Oráculo self-host distribution final | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-010` | Oráculo self-host final certification | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-011` | Oráculo self-host first generation | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-012` | Oráculo self-host fixed point | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-013` | Oráculo self-host frontend | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-014` | Oráculo self-host functional equivalence | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-015` | Oráculo self-host generics aot | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-016` | Oráculo self-host image replay retirement | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-017` | Oráculo self-host interop base | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-018` | Oráculo self-host interop parity | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-019` | Oráculo self-host ir | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-020` | Oráculo self-host llvm backend | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-021` | Oráculo self-host llvm c api | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-022` | Oráculo self-host low level parity | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-023` | Oráculo self-host module set | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-024` | Oráculo self-host native text | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-025` | Oráculo self-host object model | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-026` | Oráculo self-host performance | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-027` | Oráculo self-host program ir | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-028` | Oráculo self-host project driver | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-029` | Oráculo self-host promotion readiness | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-030` | Oráculo self-host README | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-031` | Oráculo self-host repeated fixed point | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-032` | Oráculo self-host robustness final | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-033` | Oráculo self-host robustness | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-034` | Oráculo self-host runtime bootstrap | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-035` | Oráculo self-host runtime library | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-036` | Oráculo self-host self hosted generation | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-037` | Oráculo self-host semantic | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-038` | Oráculo self-host source bool void | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-039` | Oráculo self-host source control flow | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-040` | Oráculo self-host source derived backend | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-041` | Oráculo self-host source expressions | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-042` | Oráculo self-host source floating scalars | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-043` | Oráculo self-host source functions | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-044` | Oráculo self-host source graph | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-045` | Oráculo self-host source integer widths | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-046` | Oráculo self-host source loop control | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-047` | Oráculo self-host source pointers | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |
| `C12B-SHC-048` | Oráculo self-host specialized code | `self-host` | contrato JSON o explicación normativa | campos tipados, cardinalidades, paths y expectativas declaradas |

## 5. Invariantes y fallos

- Ningún consumidor se diagnostica mientras falte su productor.
- Un path, hash, versión o artifact incidental sólo es oráculo cuando el contrato lo declara semántico.
- No se usa cache, fallback, producto viejo ni conteo rígido para fabricar éxito.
- La evidencia upstream no se reetiqueta como cobertura propia.

## 6. Estrategia de prueba futura

- Casos PR: 240.
- Casos NIGHTLY/weekly: 48.
- Los casos de integración usan el compilador construido y programas `.ns` mínimos; los gates self-host se reservan para frecuencia nocturna o semanal.
- Los upstream reciben smoke de construcción/enlace desde BuildSystem, no duplicación de sus suites internas.

## 7. Reconstructibilidad

La revisión conserva archivos, símbolos, contratos, errores, dependencias, oráculos y casos suficientes para implementar la suite sin redescubrir la intención.