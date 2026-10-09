# Tanda 01.35 — Layout de flexible arrays de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CF-TGT-016`, casos `TF-TGT-121..128` |
| Producto | Observar `StructFieldRequest::isFlexibleArray` en `layoutStruct`; no modificar |
| Excluido | Bit-fields válidos, unions y cambios a la regla vigente de posición |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- un único flexible array 0/1 produce extent cero, tamaño cero, alineación 1 y offset cero;
- flexible arrays con extent cero o especificado conservan el cursor y el extent exactos aun cuando no sean el último campo;
- una instancia nueva funciona sin metadata ni estado externo;
- alineación cero, alineación no potencia de dos y combinación con bit-field producen `nullopt`;
- agregado vacío, flexible arrays cero duplicados, extent uno y offset explícito máximo conservan el resultado límite exacto;
- 180 combinaciones de posición, tamaño, alineación y `pack` coinciden con un oráculo independiente;
- repetición y cambio del orden de invocación no alteran los resultados de cinco fixtures;
- 65,536 campos normales y flexibles intercalados coinciden con el oráculo y el overflow límite se rechaza.

El oráculo independiente implementa la colocación secuencial mediante residuo y padding, distingue el permiso de
tamaño cero del flag flexible, calcula el máximo extent y redondea al final. Es iterativo, `O(F)` y no usa recursión.

## Evidencia

- Gate `CF-TGT-016`: 8/8 en Debug (1.28 s) y Release (0.41 s).
- Suite nueva completa: Debug 34/34 en 9.06 s; Release 34/34 en 2.80 s.
- Controlador Python: 51/51; fuentes Python: 55/55 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos.
- Manifiesto: 258 casos, 0 duplicados y 8 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes protegidos conservaron exactamente sus hashes SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-121` | flexible array mínimo de extent cero |
| `TF-TGT-122` | extent cero/especificado y posición no final |
| `TF-TGT-123` | ausencia de estado externo |
| `TF-TGT-124` | alineaciones inválidas y combinación con bit-field |
| `TF-TGT-125` | vacío, duplicados y límites representables |
| `TF-TGT-126` | matriz de 180 variantes |
| `TF-TGT-127` | determinismo y orden de invocación |
| `TF-TGT-128` | escala y rechazo del overflow límite |

Diagramas: no aplica; el contrato es una colocación secuencial y una reducción lineal a extent.
