# Tanda 01.37 — Allocation units bit-field secuenciales de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CF-TGT-018`, casos `TF-TGT-137..144` |
| Producto | Observar el packing secuencial bit-field de `layoutStruct`; no modificar |
| Excluido | Barreras de ancho cero y unions bit-field |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- un bit-field 1/1 de width 1 produce unidad, offset y bit offset mínimos exactos;
- campos compatibles comparten unidad hasta llenarla y abren otra al faltar bits;
- cambiar el tamaño de allocation unit abre una unidad nueva alineada;
- una instancia nueva funciona sin metadata ni estado externo;
- alineación no potencia de dos y una secuencia cuyo extent desborda `uint64_t` producen `nullopt`;
- llenado exacto, bits restantes insuficientes, cambio de tamaño y anchos duplicados coinciden con el oráculo;
- 240 combinaciones de tamaño, width y `pack` coinciden con un oráculo independiente;
- cinco fixtures conservan packing exacto ante repetición y cambio del orden de invocación;
- 65,536 campos de un bit producen exactamente 8,192 bytes y la secuencia límite sigue rechazando overflow.

El oráculo mantiene de forma independiente offset, tamaño y bits usados de la unidad activa; abre una unidad sólo
por ausencia, cambio de tamaño o falta de capacidad. Es iterativo, `O(F)` tiempo, `O(F)` memoria y no usa recursión.

## Evidencia

- Gate `CF-TGT-018`: 8/8 en Debug (1.37 s) y Release (0.42 s).
- Suite nueva completa: Debug 36/36 en 10.23 s; Release 36/36 en 3.10 s.
- Controlador Python: 51/51; fuentes Python: 57/57 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos.
- Manifiesto: 274 casos, 0 duplicados y 8 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-137` | allocation unit mínima |
| `TF-TGT-138` | compartir, llenar y abrir unidades |
| `TF-TGT-139` | ausencia de estado externo |
| `TF-TGT-140` | alineación inválida y overflow de extent |
| `TF-TGT-141` | límites de capacidad y cambio de tamaño |
| `TF-TGT-142` | matriz de 240 variantes |
| `TF-TGT-143` | determinismo y orden de invocación |
| `TF-TGT-144` | escala y overflow límite |

Diagramas: no aplica; el oráculo expresa una máquina de estado lineal de una sola unidad activa.
