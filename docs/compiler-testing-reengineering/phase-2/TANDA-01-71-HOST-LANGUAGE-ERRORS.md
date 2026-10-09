# Tanda 01.71 - Host.Language: errores, never y pánico

Fecha: 2026-10-09

## Alcance cerrado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-007` |
| Casos implementados | `TL-LANG-088` a `TL-LANG-097` (10 casos) |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | LISTO |

La tanda caracteriza propagación `?`, conversiones de error, contratos `throws`, cobertura
de catches, coerción y unificación de `never`, perfiles de pánico y seguridad de
aserciones. Todas las comprobaciones usan la API pública y entradas aisladas.

## Contratos comprobados

| Caso | Comportamiento verificado |
|---|---|
| `TL-LANG-088` | Protocolo Result completo, aceptación, limpieza del error e input inmutable. |
| `TL-LANG-089` | Success/error vacíos, tres miembros ausentes y omisiones combinadas. |
| `TL-LANG-090` | Identidad, diferencia, adapter y tipos vacíos en la matriz de conversión. |
| `TL-LANG-091` | Ambos ABI de throws, tipo vacío, frontera C y adapter, con prioridad diagnóstica. |
| `TL-LANG-092` | Declared vacío, subset, conjunto exacto, superset, duplicados y rethrow. |
| `TL-LANG-093` | Coerción de never hacia destino vacío y cinco destinos representativos. |
| `TL-LANG-094` | Breaks vacíos, sólo never, tipo único, duplicados, mezcla y conflicto. |
| `TL-LANG-095` | Cuatro spellings válidos y siete entradas inválidas por case, grammar o trailing. |
| `TL-LANG-096` | Nombre, cleanup y cruce FFI de los perfiles abort y native unwind. |
| `TL-LANG-097` | Cuatro clases de aserción y fallback vigente para un enum externo. |

## Propiedades adicionales verificadas

- La validación exitosa elimina cualquier diagnóstico anterior.
- El tipo de error ausente precede al rechazo por frontera C sin adapter.
- Los catches emplean semántica de conjunto: duplicados y orden no alteran la cobertura.
- Un conjunto de breaks vacío o compuesto sólo por `never` unifica a `never`.
- Los perfiles de pánico no aceptan espacios ni case arbitrario.
- Ningún perfil de pánico puede cruzar FFI.
- Sólo `Assume` requiere contexto unsafe en el contrato vigente.
- No se introdujo recursión.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido completo: Debug 10/10 en 4.45 s; Release 10/10 en 0.56 s.
- Suite nueva completa: Debug 67/67 en 74.56 s; Release 67/67 en 9.08 s.
- Controlador Python: 51/51 en 0.677 s.
- Fuentes Python de la suite: 88/88 compiladas en memoria.
- Manifiesto: 539 casos, 0 duplicados y 10 casos de `CL-LANG-007`.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos; 143 avisos de
  dependencias no propias suprimidos y una supresión local para el fallback externo.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; fuentes nuevas o modificadas con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

`CL-LANG-007` queda completo: 10 casos implementados y verificados, sin hallazgos de
producto ni pendientes internos. La continuación de Ola 1 debe tomar `CL-LANG-008`.
