# Tooling — disposición y contrato reconstructivo

## 1. Estado y procedencia

- **Revisión:** `93094d49210999de5018765df1c7551e942f43f4`.
- **Universo:** 15 archivos, 4418 líneas, 168 declaraciones/artefactos.
- **Disposición:** EXCLUIDA=15.
- **Diseño futuro:** 0 contratos y 0 casos; ninguna prueba fue creada o ejecutada.

## 2. Propósito y frontera

Mantener utilidades de catálogo, migración, medición y conformance que no forman parte del comportamiento del compilador.

**Dependencias:** Python;documentos y manifests del repositorio.

`DIRECTA` significa comportamiento propio aislable; `INDIRECTA`, comportamiento observable mediante integración o gate; `UPSTREAM`, responsabilidad del proveedor más smoke de integración; `EXCLUIDA`, fuera de la suite NSC con razón persistida.

## 3. Inventario archivo por archivo

| ID | Ruta | Cat. | Disp. | Contrato | Propósito |
|---|---|---|---|---|---|
| PF-000960 | `tools/audit_test_execution_parity.py` | `SUPPORT_TOOL` | `EXCLUIDA` | `` | Herramienta de soporte audit test execution parity; queda fuera de la suite de comportamiento de NSC y requiere, si se mantiene, pruebas propias del tooling. |
| PF-000961 | `tools/build_project.py` | `SUPPORT_TOOL` | `EXCLUIDA` | `` | Herramienta de soporte build project; queda fuera de la suite de comportamiento de NSC y requiere, si se mantiene, pruebas propias del tooling. |
| PF-000962 | `tools/clean_project.py` | `SUPPORT_TOOL` | `EXCLUIDA` | `` | Herramienta de soporte clean project; queda fuera de la suite de comportamiento de NSC y requiere, si se mantiene, pruebas propias del tooling. |
| PF-000963 | `tools/generate_native_sharp_3_status_views.py` | `SUPPORT_TOOL` | `EXCLUIDA` | `` | Herramienta de soporte generate native sharp 3 status views; queda fuera de la suite de comportamiento de NSC y requiere, si se mantiene, pruebas propias del tooling. |
| PF-000964 | `tools/generate_native_sharp_3_tracking.py` | `SUPPORT_TOOL` | `EXCLUIDA` | `` | Herramienta de soporte generate native sharp 3 tracking; queda fuera de la suite de comportamiento de NSC y requiere, si se mantiene, pruebas propias del tooling. |
| PF-000965 | `tools/generate_semantic_performance_diagrams.py` | `SUPPORT_TOOL` | `EXCLUIDA` | `` | Herramienta de soporte generate semantic performance diagrams; queda fuera de la suite de comportamiento de NSC y requiere, si se mantiene, pruebas propias del tooling. |
| PF-000966 | `tools/generate_test_destination_matrix.py` | `SUPPORT_TOOL` | `EXCLUIDA` | `` | Herramienta de soporte generate test destination matrix; queda fuera de la suite de comportamiento de NSC y requiere, si se mantiene, pruebas propias del tooling. |
| PF-000967 | `tools/generate_transitional_unit_adapters.py` | `SUPPORT_TOOL` | `EXCLUIDA` | `` | Herramienta de soporte generate transitional unit adapters; queda fuera de la suite de comportamiento de NSC y requiere, si se mantiene, pruebas propias del tooling. |
| PF-000968 | `tools/measure_test_infrastructure.py` | `SUPPORT_TOOL` | `EXCLUIDA` | `` | Herramienta de soporte measure test infrastructure; queda fuera de la suite de comportamiento de NSC y requiere, si se mantiene, pruebas propias del tooling. |
| PF-000969 | `tools/native_sharp_3_catalog.py` | `SUPPORT_TOOL` | `EXCLUIDA` | `` | Herramienta de soporte native sharp 3 catalog; queda fuera de la suite de comportamiento de NSC y requiere, si se mantiene, pruebas propias del tooling. |
| PF-000970 | `tools/native_sharp_3_source_inventory.py` | `SUPPORT_TOOL` | `EXCLUIDA` | `` | Herramienta de soporte native sharp 3 source inventory; queda fuera de la suite de comportamiento de NSC y requiere, si se mantiene, pruebas propias del tooling. |
| PF-000971 | `tools/ns3_conformance.py` | `SUPPORT_TOOL` | `EXCLUIDA` | `` | Herramienta de soporte ns3 conformance; queda fuera de la suite de comportamiento de NSC y requiere, si se mantiene, pruebas propias del tooling. |
| PF-000972 | `tools/nsc_performance_baseline.py` | `SUPPORT_TOOL` | `EXCLUIDA` | `` | Herramienta de soporte nsc performance baseline; queda fuera de la suite de comportamiento de NSC y requiere, si se mantiene, pruebas propias del tooling. |
| PF-000973 | `tools/verify_native_sharp_3_catalog.py` | `SUPPORT_TOOL` | `EXCLUIDA` | `` | Herramienta de soporte verify native sharp 3 catalog; queda fuera de la suite de comportamiento de NSC y requiere, si se mantiene, pruebas propias del tooling. |
| PF-000974 | `tools/verify_native_sharp_3_plan.py` | `SUPPORT_TOOL` | `EXCLUIDA` | `` | Herramienta de soporte verify native sharp 3 plan; queda fuera de la suite de comportamiento de NSC y requiere, si se mantiene, pruebas propias del tooling. |

## 4. Contratos mantenidos

| Contrato | Nombre | Capa | Entradas | Oráculo |
|---|---|---|---|---|
| — | No se deriva contrato directo de NSC; la disposición por archivo es el cierre. | — | — | — |

## 5. Invariantes y fallos

- Ningún consumidor se diagnostica mientras falte su productor.
- Un path, hash, versión o artifact incidental sólo es oráculo cuando el contrato lo declara semántico.
- No se usa cache, fallback, producto viejo ni conteo rígido para fabricar éxito.
- La evidencia upstream no se reetiqueta como cobertura propia.

## 6. Estrategia de prueba futura

- Casos PR: 0.
- Casos NIGHTLY/weekly: 0.
- Los casos de integración usan el compilador construido y programas `.ns` mínimos; los gates self-host se reservan para frecuencia nocturna o semanal.
- Los upstream reciben smoke de construcción/enlace desde BuildSystem, no duplicación de sus suites internas.

## 7. Reconstructibilidad

La revisión conserva la razón completa de exclusión/upstream y su frontera de integración; no afirma reconstructibilidad interna del código ajeno.