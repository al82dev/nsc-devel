# Tanda 01.60 — Etiquetas y notas de Host.Diagnostics

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\\nsc-devel` |
| Contrato | `CD-DIA-011`, casos `TD-DIA-064..069` |
| Producto | Observar etiquetas secundarias y notas de `DiagnosticRenderer::render`; no modificar |
| Excluido | Encabezado, marcador, renderizado de motores completos y correcciones del producto |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- etiqueta en la misma fuente con posición propia y severidad textual `note`;
- etiqueta en otra fuente resuelta contra su propio rango;
- etiqueta con archivo inválido y fallback `<source>:1:1`;
- dos etiquetas y dos notas con orden estable por vector y etiquetas antes que notas;
- notas sin etiquetas, con prefijo, bytes y orden exactos;
- retorno temprano de la línea primaria que omite etiquetas y notas según la conducta vigente.

La salida observada es lineal en el número y longitud de etiquetas y notas. Los casos son iterativos y no usan
recursión.

## Implementación de pruebas

- `DiagnosticRendererSecondaryModule.cpp` registra seis casos dentro del ejecutable consolidado.
- `run_host_diagnostics_renderer_secondary_gate.py` exige exactamente `TD-DIA-064..069` y seis aprobaciones.
- `cases.jsonl` conserva la trazabilidad `case_id -> CD-DIA-011 -> Host.Diagnostics` en Debug y Release.
- El renderizador se observa mediante la misma fuente de producto ya enlazada, sin modificarla.

## Evidencia

- Gate `CD-DIA-011`: 6/6 en Debug (2.26 s) y Release (0.44 s).
- Suite nueva completa: Debug 59/59 en 48.48 s; Release 59/59 en 9.28 s.
- Controlador Python: 51/51 en 0.768 s; fuentes Python: 80/80 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en código de usuario.
- Manifiesto: 439 casos, 0 duplicados y 6 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TD-DIA-064` | etiqueta en la misma fuente con posición propia |
| `TD-DIA-065` | etiqueta en otra fuente |
| `TD-DIA-066` | fallback de una etiqueta con archivo inválido |
| `TD-DIA-067` | orden de dos etiquetas y dos notas |
| `TD-DIA-068` | notas sin etiquetas |
| `TD-DIA-069` | retorno temprano que omite toda salida secundaria |

Diagramas: no aplica; el contrato es una secuencia textual lineal.
