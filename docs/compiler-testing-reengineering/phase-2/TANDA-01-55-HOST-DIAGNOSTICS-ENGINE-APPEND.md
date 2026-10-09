# Tanda 01.55 — Fidelidad de append de Host.Diagnostics

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CD-DIA-006`, casos `TD-DIA-027..031` |
| Producto | Observar las sobrecargas `append` de `DiagnosticEngine`; no modificar |
| Excluido | Merge, clear, renderizado y productores de diagnósticos |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- un diagnóstico simple sin rango conserva identificador, severidad, ubicación, mensaje y ausencia de rango;
- un diagnóstico con rango conserva ambos extremos y usa el inicio como ubicación principal;
- un diagnóstico estructurado conserva rango primario, etiqueta secundaria, nota y conteo de severidad;
- la sobrecarga estructurada sin rango sintetiza un rango vacío en la ubicación sin perder datos secundarios;
- un receptor que ya alcanzó su presupuesto suprime el append, mantiene sus conteos aceptados y no altera la fuente.

Cada caso crea una fuente y un receptor independientes. Las comprobaciones son constantes, salvo la copia lineal
del contenido almacenado que pertenece al contrato observado. No existe recursión.

## Implementación de pruebas

- `DiagnosticEngineAppendModule.cpp` registra cinco casos dentro del ejecutable consolidado.
- `run_host_diagnostics_engine_append_gate.py` exige exactamente `TD-DIA-027..031` y cinco aprobaciones.
- `cases.jsonl` conserva la trazabilidad `case_id -> CD-DIA-006 -> Host.Diagnostics` en Debug y Release.
- Los casos separan las dos formas simples, las dos formas estructuradas y la frontera de supresión.
- Las comprobaciones de rangos, etiquetas y notas validan primero su presencia para evitar accesos inválidos.

## Evidencia

- Gate `CD-DIA-006`: 5/5 en Debug (1.53 s) y Release (0.39 s).
- Suite nueva completa: Debug 54/54 en 37.12 s; Release 54/54 en 8.53 s.
- Controlador Python: 51/51 en 0.704 s; fuentes Python: 75/75 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos propios.
- Manifiesto: 401 casos, 0 duplicados y 5 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- El código y la configuración legible cumplen 120 caracteres; el JSONL conserva una fila canónica por caso.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TD-DIA-027` | fidelidad del append simple sin rango |
| `TD-DIA-028` | fidelidad del append simple con rango |
| `TD-DIA-029` | fidelidad estructurada con rango, etiqueta y nota |
| `TD-DIA-030` | síntesis de rango en append estructurado sin rango |
| `TD-DIA-031` | supresión por presupuesto del receptor y fuente inmutable |

Diagramas: no aplica; las cinco filas enumeran directamente las variantes públicas de append autorizadas.
