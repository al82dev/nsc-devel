# Tanda 01.54 — Presupuesto de errores de Host.Diagnostics

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CD-DIA-005`, casos `TD-DIA-019..026` |
| Producto | Observar presupuesto y supresión de `DiagnosticEngine`; no modificar |
| Excluido | Append, merge, clear, renderizado y productores |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- un límite solicitado de cero se normaliza a uno sin alterar el estado vacío;
- el error que alcanza el límite se almacena, actualiza conteos y activa la bandera sin ser suprimido;
- un error posterior al límite no se almacena ni cuenta y aumenta suprimidos una vez;
- `Warning`, `Lint` y `Note` posteriores al límite se suprimen globalmente sin alterar sus conteos aceptados;
- dos errores con límite tres permanecen debajo del límite y no producen supresión;
- el tercer error con límite tres se almacena y sólo después deja activa la transición de límite alcanzado.

Cada reporte ejecuta una comprobación constante y, si se acepta, conserva el costo lineal del contenido almacenado.
Los reportes suprimidos no crecen el vector y actualizan un único contador en tiempo constante. No existe recursión.

## Implementación de pruebas

- `DiagnosticEngineBudgetModule.cpp` registra ocho casos dentro del ejecutable consolidado.
- `run_host_diagnostics_engine_budget_gate.py` exige exactamente `TD-DIA-019..026` y ocho aprobaciones.
- `cases.jsonl` conserva la trazabilidad `case_id -> CD-DIA-005 -> Host.Diagnostics` en Debug y Release.
- Las ocho pruebas crean motores independientes y observan sólo el prefijo aceptado, conteos y transición.
- La comprobación del prefijo verifica primero que exista un registro para no introducir acceso fuera de rango.

## Evidencia

- Gate `CD-DIA-005`: 8/8 en Debug (2.52 s) y Release (0.50 s).
- Suite nueva completa: Debug 53/53 en 35.95 s; Release 53/53 en 8.12 s.
- Controlador Python: 51/51 en 0.836 s; fuentes Python: 74/74 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos propios.
- Manifiesto: 396 casos, 0 duplicados y 8 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- El código y la configuración legible cumplen 120 caracteres; el JSONL conserva una fila canónica por caso.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TD-DIA-019` | normalización de límite cero |
| `TD-DIA-020` | aceptación del error que alcanza límite uno |
| `TD-DIA-021` | supresión del error posterior |
| `TD-DIA-022` | supresión global de Warning |
| `TD-DIA-023` | supresión global de Lint |
| `TD-DIA-024` | supresión global de Note |
| `TD-DIA-025` | estado debajo de límite tres |
| `TD-DIA-026` | aceptación y transición del tercer error |

Diagramas: no aplica; los casos enumeran directamente la máquina de dos estados del presupuesto.
