# Tanda 01.50 — Vocabulario de severidad de Host.Diagnostics

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CD-DIA-001`, casos `TD-DIA-001..002` |
| Producto | Observar `DiagnosticSeverity` y `diagnosticSeverityName`; no modificar |
| Excluido | IDs de diagnóstico, almacenamiento, presupuesto, merge y renderizado |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- `Note`, `Warning`, `Error` y `Lint` producen exactamente `note`, `warning`, `error` y `lint`;
- un valor fuera del conjunto de enumeradores produce el fallback exacto `error`;
- las comparaciones usan una tabla independiente y no derivan expectativas del producto;
- ambos casos carecen de estado compartido, asignaciones dinámicas y dependencias externas.

Cada consulta cuesta tiempo y memoria constantes. No existe recursión.

## Implementación de pruebas

- `DiagnosticSeverityModule.cpp` registra los dos casos dentro del ejecutable consolidado.
- `run_host_diagnostics_severity_gate.py` exige exactamente `TD-DIA-001..002` y dos aprobaciones.
- `cases.jsonl` conserva la trazabilidad `case_id -> CD-DIA-001 -> Host.Diagnostics` en Debug y Release.
- CMake registra el módulo y su gate dirigido sin crear un ejecutable por prueba.

El cast deliberado del caso inválido activa `clang-analyzer-optin.core.EnumCastOutOfRange`. La línea contiene una
supresión localizada y documentada porque alcanzar el fallback fuera del enum es precisamente el contrato público
congelado; no se desactiva ninguna regla global.

## Evidencia

- Gate `CD-DIA-001`: 2/2 en Debug (0.87 s) y Release (0.35 s).
- Suite nueva completa: Debug 49/49 en 22.09 s; Release 49/49 en 9.21 s.
- Controlador Python: 51/51 en 0.751 s; fuentes Python: 70/70 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos después de la supresión localizada.
- Manifiesto: 372 casos, 0 duplicados y 2 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- El código y la configuración legible cumplen 120 caracteres; el JSONL conserva una fila canónica por caso.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TD-DIA-001` | nombres exactos de las cuatro severidades conocidas |
| `TD-DIA-002` | fallback `error` para un valor fuera del enum |

Diagramas: no aplica; el contrato es una conversión pura y sin estado de un valor a una vista de cadena.
