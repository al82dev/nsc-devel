# Tanda 01.57 — Consultas y reinicio de Host.Diagnostics

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CD-DIA-008`, casos `TD-DIA-043..046` |
| Producto | Observar consultas y `DiagnosticEngine::clear`; no modificar |
| Excluido | Merge, append, renderizado y productores de diagnósticos |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- tras una mezcla de severidades, cada consulta refleja exactamente el vector y los conteos aceptados;
- `clear` elimina diagnósticos y reinicia conteos, supresiones y banderas;
- `clear` conserva el límite de errores configurado;
- después de `clear`, el motor puede aceptar otra vez hasta el mismo límite y sólo expone los registros nuevos.

Los accesores y transiciones comprobados son constantes; destruir el vector es lineal en su contenido almacenado.
Los casos son iterativos y no usan recursión.

## Implementación de pruebas

- `DiagnosticEngineStateModule.cpp` registra cuatro casos dentro del ejecutable consolidado.
- `run_host_diagnostics_engine_state_gate.py` exige exactamente `TD-DIA-043..046` y cuatro aprobaciones.
- `cases.jsonl` conserva la trazabilidad `case_id -> CD-DIA-008 -> Host.Diagnostics` en Debug y Release.
- Cada caso crea un motor independiente y comprueba exclusivamente consultas, reset y reutilización.

## Evidencia

- Gate `CD-DIA-008`: 4/4 en Debug (1.49 s) y Release (0.37 s).
- Suite nueva completa: Debug 56/56 en 40.81 s; Release 56/56 en 8.79 s.
- Controlador Python: 51/51 en 0.719 s; fuentes Python: 77/77 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos propios.
- Manifiesto: 416 casos, 0 duplicados y 4 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- El código y la configuración legible cumplen 120 caracteres; el JSONL conserva una fila canónica por caso.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TD-DIA-043` | exactitud de todos los accesores tras reportes mixtos |
| `TD-DIA-044` | reinicio completo del estado acumulado |
| `TD-DIA-045` | conservación del límite configurado |
| `TD-DIA-046` | reutilización limpia después de clear |

Diagramas: no aplica; los cuatro casos cubren directamente estado observado, reset, configuración y reutilización.
