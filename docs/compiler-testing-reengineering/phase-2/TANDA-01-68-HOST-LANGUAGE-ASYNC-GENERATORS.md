# Tanda 01.68 - Host.Language: generadores asíncronos

Fecha: 2026-10-09

## Alcance cerrado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-005` |
| Casos implementados | `TL-LANG-063` a `TL-LANG-068` (6 casos) |
| Acumulado del contrato | 19/28 casos |
| Producto modificado | No |
| Suite histórica modificada | No |
| Pendiente del contrato | `TL-LANG-069` a `TL-LANG-077` |

La subtanda cubre planificación, demanda explícita, reanudación, consumo y cancelación
temprana de generadores asíncronos. Todos los escenarios mutables se construyen desde
un estado nuevo para que las matrices no compartan efectos.

## Contratos comprobados

| Caso | Comportamiento verificado |
|---|---|
| `TL-LANG-063` | Cinco estados por ambos valores de demanda; sólo `Pending` e `ItemReady` pueden producir. |
| `TL-LANG-064` | Tres almacenamientos por ambos modos de item, con tipos, frame, estados y flags exactos. |
| `TL-LANG-065` | Tipos, suspensiones, continuidad, presencia de yield y pruebas de préstamo inválidas. |
| `TL-LANG-066` | Demanda única, bloqueo por item prestado y rechazo estable de los tres estados terminales. |
| `TL-LANG-067` | Cuatro resultados con y sin demanda, más prioridad de rechazo en estados terminales. |
| `TL-LANG-068` | Consumo único de item y cancelación desde reposo, demanda, item y estados terminales. |

## Propiedades adicionales verificadas

- El plan conserva `item`, `error`, `cancellation`, almacenamiento y política move-only.
- La traducción de `FrameStorage` a `GeneratorStorage` es exacta en los tres valores.
- Una autorreferencia fija el frame y no introduce asignación implícita en heap.
- Await y yield conservan orden y profundidad de limpieza.
- Una demanda pendiente permanece tras `Pending`; los demás resultados limpian la demanda.
- Un item listo mantiene un único préstamo pendiente hasta `takeItem` o `stopEarly`.
- Los estados terminales no cambian después de operaciones rechazadas.
- La implementación de pruebas es iterativa y no introduce recursión.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido: Debug 19/19 en 7.51 s; Release 19/19 en 0.82 s.
- Suite nueva completa: Debug 65/65 en 68.11 s; Release 65/65 en 11.28 s.
- Controlador Python: 51/51 en 0.706 s.
- Fuentes Python de la suite: 86/86 compiladas en memoria.
- Manifiesto: 510 casos, 0 duplicados y 19 casos de `CL-LANG-005`.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos; 143 avisos de
  dependencias no propias fueron suprimidos por la configuración estándar.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`.
- `git diff --check`: correcto; fuentes nuevas o modificadas con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.

## Estado

Los generadores asíncronos quedan cerrados sin hallazgos de producto. El bloque final de
`CL-LANG-005` es `TL-LANG-069..077`, correspondiente a seguridad de hilos, metadata y
alcances estructurados de tareas.
