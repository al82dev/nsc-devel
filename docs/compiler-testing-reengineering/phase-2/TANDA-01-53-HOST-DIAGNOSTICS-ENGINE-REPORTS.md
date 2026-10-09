# Tanda 01.53 — Reportes aceptados de Host.Diagnostics

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CD-DIA-004`, casos `TD-DIA-012..018` |
| Producto | Observar `DiagnosticEngine`; no modificar |
| Excluido | Supresión, append, merge, clear, renderizado y productores |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- un motor nuevo conserva vector y conteos vacíos, límite predeterminado 100 y ninguna bandera activa;
- un reporte `Error` por ubicación almacena un registro sin rango e incrementa sólo errores;
- un reporte `Warning` por rango deriva la ubicación de `range.begin()`, conserva el rango e incrementa warnings;
- un reporte `Lint` se almacena e incrementa exclusivamente lints;
- un reporte `Note` se almacena sin incrementar los tres conteos de severidad;
- `reportStructured` conserva rango primario, mensaje, etiqueta secundaria y nota;
- una mezcla bajo el límite conserva el orden de inserción y mantiene independientes los conteos.

Cada reporte aceptado agrega un registro y copia o mueve contenido en tiempo y memoria lineales respecto de los
textos y secuencias recibidos. Las consultas y actualizaciones de conteos son constantes. No existe recursión.

## Implementación de pruebas

- `DiagnosticEngineReportModule.cpp` registra siete casos dentro del ejecutable consolidado.
- `run_host_diagnostics_engine_report_gate.py` exige exactamente `TD-DIA-012..018` y siete aprobaciones.
- `cases.jsonl` conserva la trazabilidad `case_id -> CD-DIA-004 -> Host.Diagnostics` en Debug y Release.
- CMake enlaza la implementación existente de `DiagnosticEngine` sólo dentro del ejecutable de pruebas nuevo.
- Todas las lecturas de registros y secuencias comprueban primero su tamaño para evitar fallos del propio test.

## Evidencia

- Gate `CD-DIA-004`: 7/7 en Debug (2.07 s) y Release (0.49 s).
- Suite nueva completa: Debug 52/52 en 35.51 s; Release 52/52 en 8.03 s.
- Controlador Python: 51/51 en 0.679 s; fuentes Python: 73/73 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos propios.
- Manifiesto: 388 casos, 0 duplicados y 7 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- El código y la configuración legible cumplen 120 caracteres; el JSONL conserva una fila canónica por caso.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TD-DIA-012` | estado inicial completo y límite predeterminado |
| `TD-DIA-013` | almacenamiento de Error por ubicación |
| `TD-DIA-014` | almacenamiento de Warning por rango |
| `TD-DIA-015` | almacenamiento y conteo de Lint |
| `TD-DIA-016` | almacenamiento de Note sin contador propio |
| `TD-DIA-017` | fidelidad de reporte estructurado |
| `TD-DIA-018` | orden y conteos independientes bajo el límite |

Diagramas: no aplica; esta tanda comprueba transiciones simples de aceptación sin supresión.
