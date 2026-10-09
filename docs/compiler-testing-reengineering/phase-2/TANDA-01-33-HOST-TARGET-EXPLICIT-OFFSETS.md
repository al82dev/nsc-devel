# Tanda 01.33 — Offsets explícitos y solapamiento de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CF-TGT-014`, casos `TF-TGT-105..112` |
| Producto | Observar offsets explícitos del overload general de `layoutStruct`; no modificar |
| Excluido | Union, flexible arrays y bit-fields |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- un campo 4/4 en offset 1 se acepta sin override explícito y produce tamaño 8;
- offsets 8 y 4 se solapan deliberadamente y un campo implícito posterior continúa desde el final anterior;
- una instancia nueva no requiere metadata ni estado externo;
- offset 2 con mínimo de campo 4 y un final de campo fuera de `uint64_t` producen `nullopt`;
- offset cero, offsets duplicados y el mayor final representable permanecen exactos;
- 80 combinaciones de tamaño, alineación y offset coinciden con un oráculo independiente;
- repetición conserva exactamente offsets, extent, alineación y vector de campos;
- 65,536 campos superpuestos en 1,024 offsets producen tamaño 1,024 y el overflow límite sigue rechazado.

El oráculo conserva el offset solicitado, calcula el extent como máximo de los finales y sólo comprueba
desalineación cuando existe `minimumAlignment`, igual que exige el contrato. Es iterativo, `O(F)` y sin recursión.

## Evidencia

- Gate `CF-TGT-014`: 8/8 en Debug y Release.
- Suite nueva completa: Debug 32/32 en 10.28 s; Release 32/32 en 5.82 s.
- Controlador Python: 51/51; fuentes Python: 53/53 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos.
- Manifiesto: 242 casos, 0 duplicados y 8 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-105` | offset explícito mínimo |
| `TF-TGT-106` | solapamiento y continuación implícita |
| `TF-TGT-107` | ausencia de estado externo |
| `TF-TGT-108` | desalineación explícita y overflow |
| `TF-TGT-109` | cero, duplicados y límite representable |
| `TF-TGT-110` | matriz de 80 variantes |
| `TF-TGT-111` | determinismo |
| `TF-TGT-112` | escala con solapamiento |

Diagramas: no aplica; el contrato es una reducción secuencial de offsets a extent.
