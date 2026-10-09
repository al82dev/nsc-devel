# Tanda 01.72 - Host.Language: expresiones, rangos e interpolación

Fecha: 2026-10-09

## Alcance implementado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-008` |
| Casos implementados | 13/14: `TL-LANG-098`, `TL-LANG-100..111` |
| Caso bloqueado | `TL-LANG-099` |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | PARCIAL POR DEFECTO DE PRODUCTO |

La tanda caracteriza contratos de lowering, detección textual, operandos null-aware,
rangos, parsing y renderizado de interpolación. Las matrices ejercitan entradas válidas,
inválidas y de borde sin añadir lógica de producción.

## Contratos comprobados

| Caso | Comportamiento verificado |
|---|---|
| `TL-LANG-098` | Siete features con spelling y garantías de orden, evaluación, asignación y ABI exactos. |
| `TL-LANG-100` | Disponibilidad estable de los siete features declarados. |
| `TL-LANG-101` | Marcador de parameter pack en varias posiciones, vacío, cierres y trailing. |
| `TL-LANG-102` | Puntero, referencia, opcional, otro y prioridades de clasificación combinadas. |
| `TL-LANG-103` | Endpoints forward/from-end en cero, length y length+1. |
| `TL-LANG-104` | Defaults, from-end, rango vacío, orden inverso y límites inválidos. |
| `TL-LANG-105` | Literal, hole recortado y formato con parts, offsets y longitudes exactos. |
| `TL-LANG-106` | Llaves escapadas, nesting, quotes y separador de formato superior. |
| `TL-LANG-107` | Llaves, expresión, formato y quote inválidos; diagnóstico exacto y parts vacíos. |
| `TL-LANG-108` | Handler exitoso, fallo en cada part, short-circuit y diagnóstico fallback. |
| `TL-LANG-109` | Matriz de literal, bool, entero, signo, D/d/X/x, width y spellings inválidos. |
| `TL-LANG-110` | Decimal, padding, hex case, signo, cero negativo, bool y orden de literales. |
| `TL-LANG-111` | Hex negativo, width, formato, tipo y overflow inválidos con output limpio. |

## Bloqueo confirmado

`TL-LANG-099` pide caracterizar un valor externo de `ExpressionFeature`. La función
`expressionLoweringContract` convierte el enum a índice y accede directamente al arreglo
de siete contratos, sin comprobación de rango. Un valor externo produce acceso fuera de
límites y comportamiento indefinido; no existe un fallback contractual que la prueba pueda
observar de forma reproducible.

- `BLOQUEADO_POR`: ausencia de validación/fallback en el producto.
- Resolverlo exige cambiar `expressionLoweringContract` o su contrato público.
- Fase 2 no autoriza cambios de producto; no se añadió una prueba que invoque UB ni un
  oráculo falso.

## Propiedades adicionales verificadas

- Los contratos de lowering preservan orden fuente, evaluación única, ABI y cero asignación implícita.
- La clasificación textual vigente de packs acepta el marcador en cualquier posición y exige `>` final.
- Puntero prevalece sobre referencia y opcional; referencia prevalece sobre opcional.
- Un rango medio abierto vacío es válido; uno invertido no lo es.
- El parser preserva offsets de bytes del texto decodificado y elimina whitespace exterior del hole.
- Un handler deja de recibir parts inmediatamente después del primer rechazo.
- El renderer builtin borra siempre cualquier resultado parcial al fallar.
- No se introdujo recursión.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido implementable: Debug 13/13 en 5.95 s; Release 13/13 en 0.71 s.
- Suite nueva completa: Debug 68/68 en 79.27 s; Release 68/68 en 9.41 s.
- Controlador Python: 51/51 en 0.696 s.
- Fuentes Python de la suite: 89/89 compiladas en memoria.
- Manifiesto: 552 casos, 0 duplicados y 13 casos implementados de `CL-LANG-008`.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos; 143 avisos de
  dependencias no propias suprimidos.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; fuentes nuevas o modificadas con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

`CL-LANG-008` queda con 13 casos implementados y verificados y un bloqueo de producto
documentado. La Fase 2 puede continuar con contratos independientes, pero no declarar esta
ola completa mientras `TL-LANG-099` siga bloqueado.
