# Tanda 01.39 — Bit-fields en unions de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CF-TGT-020`, casos `TF-TGT-153..160` |
| Producto | Observar bit-fields con `StructLayoutOptions::isUnion`; no modificar |
| Excluido | Packing secuencial y barreras de ancho cero |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- un bit-field de union conserva unidad completa, offset cero y bit offset cero;
- varios tamaños contribuyen por máximo extent y permanecen todos en offset cero;
- la alineación natural de las unidades no eleva la alineación agregada vigente;
- una instancia nueva funciona sin metadata ni estado externo;
- width cero nombrado y width superior a la unidad se rechazan;
- width máximo exacto, campos duplicados, mínimo agregado y mayor unidad representable conservan resultados exactos;
- 120 combinaciones de tamaño, width, `pack` y mínimo agregado coinciden con un oráculo independiente;
- cinco fixtures conservan layout exacto ante repetición y cambio del orden de invocación;
- 65,536 campos producen tamaño 8, alineación agregada 1 y offsets cero, y el width inválido sigue rechazado.

El oráculo independiente trata cada miembro como una unidad completa en offset cero, reduce su tamaño al máximo
extent y aplica únicamente la alineación mínima agregada al cierre. Es iterativo, `O(F)` y no usa recursión.

## Evidencia

- Gate `CF-TGT-020`: 8/8 en Debug (1.39 s) y Release (0.44 s).
- Suite nueva completa: Debug 38/38 en 11.26 s; Release 38/38 en 3.25 s.
- Controlador Python: 51/51; fuentes Python: 59/59 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos.
- Manifiesto: 290 casos, 0 duplicados y 8 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-153` | miembro bit-field mínimo de union |
| `TF-TGT-154` | máximo extent con múltiples unidades |
| `TF-TGT-155` | ausencia de estado externo |
| `TF-TGT-156` | widths inválidos |
| `TF-TGT-157` | límites, duplicados y mínimo agregado |
| `TF-TGT-158` | matriz de 120 variantes |
| `TF-TGT-159` | determinismo y orden de invocación |
| `TF-TGT-160` | escala de 65,536 campos |

Diagramas: no aplica; el contrato es una reducción independiente de miembros al máximo extent.
