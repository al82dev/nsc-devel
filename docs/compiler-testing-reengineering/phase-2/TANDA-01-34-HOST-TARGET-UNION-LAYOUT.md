# Tanda 01.34 — Layout de unions de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CF-TGT-015`, casos `TF-TGT-113..120` |
| Producto | Observar el modo union de `layoutStruct`; no modificar |
| Excluido | Structs secuenciales, arrays, flexible arrays y bit-fields |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- una union mínima con un campo 1/1 produce tamaño 1, alineación 1 y offset cero;
- campos 1/1, 8/8 y 3/2 comparten offset cero y producen tamaño 8, alineación 8;
- una instancia nueva funciona sin metadata ni estado externo;
- tamaño cero, alineación cero y overflow del final explícito producen `nullopt`;
- la union vacía conserva 1/1, los campos duplicados comparten offset cero y el mayor miembro representable permanece exacto;
- 2,400 combinaciones de tamaños, alineaciones y `pack` coinciden con un oráculo independiente;
- la repetición conserva exactamente tamaño, alineación y layout de cada campo;
- 65,536 campos conservan offset cero, tamaño 16 y alineación 16, y el overflow límite sigue rechazado.

El oráculo independiente reduce todos los miembros al máximo extent, limita la alineación efectiva con
`pack` y redondea el tamaño final mediante residuo y padding. Es iterativo, `O(F)` y no usa recursión.

## Evidencia

- Gate `CF-TGT-015`: 8/8 en Debug (1.22 s) y Release (0.43 s).
- Suite nueva completa: Debug 33/33 en 10.70 s; Release 33/33 en 5.72 s.
- Controlador Python: 51/51; fuentes Python: 54/54 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos.
- Manifiesto: 250 casos, 0 duplicados y 8 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes protegidos conservaron exactamente sus hashes SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-113` | union mínima |
| `TF-TGT-114` | offsets cero y máximo extent |
| `TF-TGT-115` | ausencia de estado externo |
| `TF-TGT-116` | entradas inválidas y overflow |
| `TF-TGT-117` | vacío, duplicados y límite representable |
| `TF-TGT-118` | matriz de 2,400 variantes |
| `TF-TGT-119` | determinismo |
| `TF-TGT-120` | escala y rechazo del overflow límite |

Diagramas: no aplica; el contrato es una reducción lineal de miembros a extent y alineación.
