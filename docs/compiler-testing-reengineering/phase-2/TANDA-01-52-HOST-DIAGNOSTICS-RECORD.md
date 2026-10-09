# Tanda 01.52 — Registro propietario de Host.Diagnostics

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CD-DIA-003`, casos `TD-DIA-007..011` |
| Producto | Observar `Diagnostic` y `DiagnosticLabel`; no modificar |
| Excluido | Presupuesto, merge, renderizado, productores y políticas de validación |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- un diagnóstico sólo con ubicación conserva ID, severidad, ubicación, mensaje y rango ausente;
- un diagnóstico con rango conserva exactamente ambos extremos por valor;
- mensaje, etiquetas y notas siguen válidos después de terminar la vida de sus entradas temporales;
- una etiqueta secundaria conserva conjuntamente su rango y su mensaje;
- una ubicación primaria y un rango deliberadamente incoherentes se almacenan sin rechazo ni normalización.

El contrato contiene únicamente construcción y lectura de valores propios. La construcción cuesta tiempo y memoria
lineales en el texto y las secuencias entregadas; las lecturas son constantes. No existe recursión.

## Implementación de pruebas

- `DiagnosticRecordModule.cpp` registra cinco casos dentro del ejecutable consolidado.
- `run_host_diagnostics_record_gate.py` exige exactamente `TD-DIA-007..011` y cinco aprobaciones.
- `cases.jsonl` conserva la trazabilidad `case_id -> CD-DIA-003 -> Host.Diagnostics` en Debug y Release.
- CMake enlaza la implementación existente de `Diagnostic` sólo dentro del ejecutable de pruebas nuevo.
- Las aserciones de secuencias comprueban primero su tamaño para registrar fallos sin acceso fuera de rango.

El primer intento del gate rechazó las clases no canónicas `lifetime` e `invalid-preserved`. Las dos filas nuevas se
corrigieron a `boundary` e `invalid`; no se modificó el esquema, el controlador ni el producto.

## Evidencia

- Gate `CD-DIA-003`: 5/5 en Debug (1.52 s) y Release (0.39 s).
- Suite nueva completa: Debug 51/51 en 32.33 s; Release 51/51 en 7.73 s.
- Controlador Python: 51/51 en 0.660 s; fuentes Python: 72/72 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos propios.
- Manifiesto: 381 casos, 0 duplicados y 5 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- El código y la configuración legible cumplen 120 caracteres; el JSONL conserva una fila canónica por caso.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TD-DIA-007` | campos básicos y ausencia de rango |
| `TD-DIA-008` | conservación exacta del rango primario |
| `TD-DIA-009` | ownership y orden después de morir las entradas |
| `TD-DIA-010` | rango y mensaje de una etiqueta secundaria |
| `TD-DIA-011` | conservación sin validación de datos incoherentes |

Diagramas: no aplica; el contrato es un registro propietario sin estados ni transiciones.
