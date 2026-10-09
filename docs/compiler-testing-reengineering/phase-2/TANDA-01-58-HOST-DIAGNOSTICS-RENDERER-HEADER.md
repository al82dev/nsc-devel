# Tanda 01.58 — Encabezado de Host.Diagnostics

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CD-DIA-009`, casos `TD-DIA-047..052` |
| Producto | Observar el encabezado de `DiagnosticRenderer::render`; no modificar |
| Excluido | Línea, marcador, etiquetas, notas y renderizado de motores completos |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- fuente válida: ruta, línea, columna, severidad y mensaje exactos;
- ubicación inválida: fallback `<source>:1:1`;
- nombres exactos para `Error`, `Warning`, `Lint` y `Note`;
- corchetes para exactamente los cinco IDs NS codificados, sin espacio adicional;
- ausencia de corchetes para un ID normal y uno desconocido;
- conservación byte a byte de mensajes vacíos y con puntuación, incluido el newline final.

El formateo es lineal en la longitud de ruta, código y mensaje. Los casos son iterativos y no usan recursión.

## Implementación de pruebas

- `DiagnosticRendererHeaderModule.cpp` registra seis casos dentro del ejecutable consolidado.
- `run_host_diagnostics_renderer_header_gate.py` exige exactamente `TD-DIA-047..052` y seis aprobaciones.
- `cases.jsonl` conserva la trazabilidad `case_id -> CD-DIA-009 -> Host.Diagnostics` en Debug y Release.
- La fuente del renderizador se enlaza al ejecutable consolidado sin modificarla.
- El cast intencional del ID desconocido posee una única supresión `NOLINTNEXTLINE` documentada.

## Evidencia

- Gate `CD-DIA-009`: 6/6 en Debug (1.94 s) y Release (0.44 s).
- Suite nueva completa: Debug 57/57 en 42.67 s; Release 57/57 en 9.01 s.
- Controlador Python: 51/51 en 0.674 s; fuentes Python: 78/78 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos; una supresión local intencional.
- Manifiesto: 422 casos, 0 duplicados y 6 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TD-DIA-047` | encabezado exacto con fuente válida e ID normal |
| `TD-DIA-048` | fallback de ubicación inválida |
| `TD-DIA-049` | nombres de las cuatro severidades |
| `TD-DIA-050` | códigos de los cinco IDs NS |
| `TD-DIA-051` | IDs normal y desconocido sin código |
| `TD-DIA-052` | bytes del mensaje y newline final |

Diagramas: no aplica; el contrato es una secuencia textual lineal.
