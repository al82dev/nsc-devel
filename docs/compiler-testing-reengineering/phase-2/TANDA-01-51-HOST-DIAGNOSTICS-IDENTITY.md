# Tanda 01.51 — Catálogo de identidad de Host.Diagnostics

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CD-DIA-002`, casos `TD-DIA-003..006` |
| Producto | Observar `DiagnosticId`, `diagnosticIdName` e `isNativeSharp3Diagnostic`; no modificar |
| Excluido | Almacenamiento, presupuesto, merge, renderizado y semántica de productores |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- una tabla independiente enumera los 69 identificadores ordinarios y sus nombres completos exactos;
- otra tabla enumera los cinco identificadores históricos clasificados y sus códigos cortos exactos;
- el catálogo combinado contiene exactamente 74 nombres y todos son únicos;
- los 69 identificadores ordinarios producen `false` en `isNativeSharp3Diagnostic`;
- los cinco identificadores de código corto producen `true` y conservan `NS3010`, `NS3020`, `NS3030`,
  `NS3049` y `NS3099`;
- un valor fuera del conjunto produce `UnknownDiagnostic` y no pertenece al subconjunto clasificado.

La comprobación de nombres y clasificaciones cuesta tiempo lineal. La unicidad usa un índice hash con memoria
lineal, evitando una comparación cuadrática. No existe recursión.

## Implementación de pruebas

- `DiagnosticIdentityModule.cpp` registra los cuatro casos dentro del ejecutable consolidado.
- `run_host_diagnostics_identity_gate.py` exige exactamente `TD-DIA-003..006` y cuatro aprobaciones.
- `cases.jsonl` conserva la trazabilidad `case_id -> CD-DIA-002 -> Host.Diagnostics` en Debug y Release.
- CMake registra el módulo y su gate dirigido sin crear un ejecutable por prueba.

El cast deliberado del valor desconocido activa `clang-analyzer-optin.core.EnumCastOutOfRange`. La línea contiene
una supresión localizada y documentada porque alcanzar el fallback fuera del enum es el contrato congelado; no se
desactiva ninguna regla global.

## Evidencia

- Gate `CD-DIA-002`: 4/4 en Debug (2.15 s) y Release (1.38 s).
- Suite nueva completa: Debug 50/50 en 22.40 s; Release 50/50 en 7.76 s.
- Controlador Python: 51/51 en 0.734 s; fuentes Python: 71/71 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos después de la supresión localizada.
- Manifiesto: 376 casos, 0 duplicados y 4 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- El código y la configuración legible cumplen 120 caracteres; el JSONL conserva una fila canónica por caso.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TD-DIA-003` | nombres exactos y únicos de los 74 identificadores |
| `TD-DIA-004` | clasificación negativa de los 69 identificadores ordinarios |
| `TD-DIA-005` | clasificación positiva y nombres cortos de los cinco identificadores NS |
| `TD-DIA-006` | fallback y clasificación de un valor desconocido |

Diagramas: no aplica; el contrato es un catálogo puro y sin estado con dos particiones explícitas.
