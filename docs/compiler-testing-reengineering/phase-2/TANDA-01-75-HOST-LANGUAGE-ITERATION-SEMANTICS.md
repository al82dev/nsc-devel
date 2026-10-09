# Tanda 01.75 - Host.Language: iteración, rangos y generator ABI

Fecha: 2026-10-09

## Alcance implementado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-011` |
| Casos implementados | 13: `TL-LANG-142..154` |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | COMPLETO |

La tanda cubre el protocolo de iteración, rangos enteros perezosos, política de mutación y
asignación, layout del frame de generadores, ABI de suspensión/cancelación/reanudación y
análisis causal de fusión. Los oráculos comparan decisiones, errores, estados, offsets y órdenes
exactos; no se infieren resultados a partir de texto incidental.

## Contratos comprobados

| Caso | Comportamiento verificado |
|---|---|
| `TL-LANG-142` | Protocolo válido y ausencia individual de item, `MoveNext`, `Current` o cleanup. |
| `TL-LANG-143` | Lending rechazado por valor y aceptado por referencia compartida o mutable. |
| `TL-LANG-144` | Rangos ascendentes/descendentes, inclusivos/exclusivos, vacíos y endpoint no alineado. |
| `TL-LANG-145` | Step cero, ambas direcciones contrarias y overflow inclusivo de ancho completo. |
| `TL-LANG-146` | Cero materialización de range y cero asignación implícita para los tres storages. |
| `TL-LANG-147` | Mutación estructural prohibida sólo durante lending; mutación de elemento permitida. |
| `TL-LANG-148` | Frame legacy vacío y poblado, locals vivos/no vivos, alignment, pinning e inválidos. |
| `TL-LANG-149` | Plan válido en cuatro storages con frame, slots, liveness, resume y cleanup exactos. |
| `TL-LANG-150` | Yield ausente, item, estados, borrow, local, intervalo y overflow inválidos. |
| `TL-LANG-151` | Drop order de todos los estados productivos y estados entry/completed/inválidos. |
| `TL-LANG-152` | Resume desde entry y yields; completed, inválido y plan vacío devuelven `nullopt`. |
| `TL-LANG-153` | Fusión válida y seis rechazos superpuestos en la prioridad contractual exacta. |
| `TL-LANG-154` | `mayFuse` coincide con el análisis causal para pares compatibles e incompatibles. |

## Propiedades adicionales verificadas

- El error de protocolo requerido se conserva para cada campo faltante y se limpia al aceptar.
- Los endpoints iguales distinguen correctamente rango vacío e inclusivo de un elemento.
- El planner conserva el storage explícito y nunca eleva el frame a heap implícito.
- Sólo los locals `livesAcrossYield` reciben slot; offsets, intervalos y cleanup depth son estables.
- La cancelación destruye únicamente slots vivos con `requiresDrop`, en orden inverso.
- La categoría resultante es multipass únicamente cuando fuente y adaptador son multipass.
- No se introdujo recursión.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido final: Debug 13/13 en 6.85 s; Release 13/13 en 0.70 s.
- Suite nueva completa final: Debug 71/71 en 311.76 s; Release 71/71 en 34.59 s.
- Controlador Python: 51/51 en 0.639 s.
- Fuentes Python de la suite: 92/92 compiladas en memoria.
- Manifiesto: 593 casos, 0 duplicados y 13 casos de `CL-LANG-011` en orden canónico.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en código de usuario.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; C++ y Python añadidos con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

`CL-LANG-011` queda completo con 13/13 casos implementados y verificados, sin bloqueos de
producto. La continuación de Ola 1 debe tomar `CL-LANG-012`.
