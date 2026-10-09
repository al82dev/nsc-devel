# Tanda 01.31 — Colocación packed de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CF-TGT-012`, casos `TF-TGT-089..096` |
| Producto | Observar el overload general de `layoutStruct`; no modificar |
| Excluido | Union, offsets explícitos, overrides, flexible arrays y bit-fields |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- pack 1 reduce un campo 8/8 a alineación efectiva 1;
- pack 2 sobre 1/1, 8/8 y 2/2 produce offsets 0, 2 y 10, tamaño 12 y alineación 2;
- una instancia nueva no requiere estado externo;
- tamaño/alineación cero y overflow de offset o final producen `nullopt`;
- pack natural, pack 1 y la mayor potencia de dos representable conservan sus resultados exactos;
- 2,400 combinaciones de dos campos y seis packs coinciden con un oráculo independiente;
- repetición conserva byte por byte el layout lógico;
- 65,536 campos con pack 1 producen tamaño 406,318, alineación 1 y último offset 406,317; un caso cercano al
  máximo produce `nullopt`.

El oráculo calcula alineación mediante residuo y padding, limita la alineación efectiva con `min(natural, pack)`
y comprueba cada suma antes de ejecutarla. Es iterativo, `O(F)` en tiempo, `O(F)` por el resultado y sin recursión.

## Evidencia

- Gate `CF-TGT-012`: 8/8 en Debug y Release.
- Suite nueva completa: Debug 30/30 en 28.96 s; Release 30/30 en 11.31 s.
- Controlador Python: 51/51; fuentes Python: 51/51 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos.
- Manifiesto: 226 casos, 0 duplicados, 8 casos del contrato.
- Builds con warnings como errores; no se modificó código de producto.
- Los ejecutables anteriores conservaron importación exclusiva de `KERNEL32.dll`; esta tanda no cambió enlace.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-089` | mínimo packed |
| `TF-TGT-090` | secuencia representativa |
| `TF-TGT-091` | ausencia de prerrequisito externo |
| `TF-TGT-092` | campos y aritmética inválidos |
| `TF-TGT-093` | packs frontera |
| `TF-TGT-094` | matriz de 2,400 variantes |
| `TF-TGT-095` | determinismo |
| `TF-TGT-096` | escala y overflow límite |

Diagramas: no aplica; es un recorrido secuencial sin estado.
