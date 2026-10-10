# Validación 08 — Bloqueo causal de consumidores self-host

## Propósito

Evitar que un fixture self-host que no pudo compilar se vuelva a construir para
cada consumidor y preservar en el reporte el grafo causal productor–consumidor.

## SCOPE LOCK

- Repositorio: `A:\nsc-devel`.
- Componente: ejecución self-host de Builder 3.
- Archivo funcional: `nscbuilder3.py`.
- Símbolos: `TERMINAL_FAILURES`, `ExecutionEngine.__init__`,
  `ExecutionEngine._run_selfhost` y el color de estado de la GUI.
- Protegidos: fixtures, manifiestos, casos, oráculos, NSC, CMake, suite
  histórica, seis archivos preexistentes, `C:`, despliegues y remotos.
- Validación funcional: pendiente de la siguiente corrida externa.

## Inventario del productor

| Proyecto | Casos lógicos | Variantes concretas |
|---|---:|---:|
| `SelfHostBasic.nsproj` | 27 | 216 |
| `SelfHostDiagnostics.nsproj` | 28 | 224 |
| `SelfHostLexer.nsproj` | 53 | 424 |
| `SelfHostSource.nsproj` | 48 | 384 |
| `SelfHostTypes.nsproj` | 36 | 288 |
| **Total** | **192** | **1,536** |

Los nombres de proyecto son únicos: `5/5` stems y `0` colisiones. La clave de
productor es `(fixture_project, build_mode, optimization)`, que produce 40
combinaciones posibles.

## Cadena causal

1. `_prepare_selfhost_fixture` ya conservaba únicamente productores exitosos.
2. Un fallo no entraba al cache.
3. El siguiente consumidor de la misma clave volvía a compilar exactamente el
   mismo proyecto con la misma configuración.
4. Una sola causa podía generar decenas de compilaciones idénticas y ocultar la
   relación causal entre el productor y sus consumidores.

## Corrección

- El primer fallo conserva `INFRASTRUCTURE_ERROR` y la ruta a `compile.log`.
- La causa se conserva por clave de productor.
- Cada consumidor posterior conserva una fila propia con estado `BLOCKED` y
  detalle `BLOQUEADO_POR fixture ...`.
- `BLOCKED` es terminal: falla el resultado global, aparece en el reporte
  condensado, admite reintento y tiene representación visible en la GUI.
- Un nuevo intento crea un motor nuevo y vuelve a intentar el productor; el
  bloqueo no sobrevive indebidamente entre corridas.

## Evidencia estática

- Proyectos productores: `5/5`.
- Consumidores lógicos: `192/192`.
- Variantes concretas: `1,536/1,536`.
- Reintentos máximos de un productor fallido por corrida: `1`.
- Consumidores eliminados u omitidos: `0`.
- Casos, fixtures y oráculos modificados: `0`.
- Recursión añadida: `0`.
- Complejidad de consulta: `O(1)` promedio por consumidor.

## Estado

`CIERRE ESTÁTICO COMPLETO; VALIDACIÓN DE EJECUCIÓN PENDIENTE DEL USUARIO`.
