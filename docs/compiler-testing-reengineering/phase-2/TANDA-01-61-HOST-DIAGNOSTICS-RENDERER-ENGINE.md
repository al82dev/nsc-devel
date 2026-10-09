# Tanda 01.61 — Motor completo de Host.Diagnostics

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\\nsc-devel` |
| Contrato | `CD-DIA-012`, casos `TD-DIA-070..075` |
| Producto | Observar `DiagnosticRenderer::renderAll`; no modificar |
| Excluido | Correcciones gramaticales del resumen, cambios del motor y renderizado individual ya cerrado |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- motor vacío con salida vacía;
- tres diagnósticos renderizados en el orden exacto del vector;
- ausencia del resumen cuando no existen supresiones;
- resumen con cantidades exactas de errores aceptados y diagnósticos suprimidos;
- conservación de los literales plurales vigentes cuando ambas cantidades son uno;
- propagación de una excepción de stream sin mutar el estado del motor.

El renderizado completo es lineal en los diagnósticos y bytes emitidos. Los casos son iterativos y no usan
recursión.

## Implementación de pruebas

- `DiagnosticRendererEngineModule.cpp` registra seis casos dentro del ejecutable consolidado.
- `run_host_diagnostics_renderer_engine_gate.py` exige exactamente `TD-DIA-070..075` y seis aprobaciones.
- `cases.jsonl` conserva la trazabilidad `case_id -> CD-DIA-012 -> Host.Diagnostics` en Debug y Release.
- El caso de recuperación utiliza un stream que falla al escribir y verifica el estado posterior del motor.
- El producto observado no fue modificado para alterar sus literales ni su política de resumen.

## Evidencia

- Gate `CD-DIA-012`: 6/6 en Debug (2.23 s) y Release (0.45 s).
- Suite nueva completa sobre el código final: Debug 60/60 en 48.26 s; Release 60/60 en 9.58 s.
- Controlador Python: 51/51 en 0.764 s; fuentes Python: 81/81 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en código de usuario.
- Manifiesto: 445 casos, 0 duplicados y 6 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TD-DIA-070` | motor vacío con salida vacía |
| `TD-DIA-071` | orden del vector de diagnósticos |
| `TD-DIA-072` | ausencia de resumen sin supresiones |
| `TD-DIA-073` | cantidades exactas del resumen |
| `TD-DIA-074` | literales plurales vigentes con cantidades uno |
| `TD-DIA-075` | propagación del stream y estado inmutable del motor |

Diagramas: no aplica; el contrato es una secuencia textual lineal.
