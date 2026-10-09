# Tanda 01.40 — Finalización de agregados y propiedades de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CF-TGT-021`, casos `TF-TGT-161..168` |
| Producto | Observar padding final y propiedades de `TargetDataLayout`; no modificar |
| Excluido | Reglas de colocación de campos ya cubiertas |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- extent 3 con alineación 2 finaliza en tamaño 4;
- un agregado representativo con mínimo 16 finaliza en 16/16;
- una instancia nueva publica alineación natural máxima 8 y little-endian;
- extent máximo con alineación 16 rechaza overflow de padding;
- agregado vacío acepta el mayor mínimo potencia de dos y extent exacto no añade padding;
- 120 combinaciones de tamaño, alineación y mínimo agregado coinciden con un oráculo de redondeo independiente;
- cinco fixtures conservan tamaño y alineación ante repetición y cambio del orden de invocación;
- 65,536 campos 1/1 finalizan exactamente en 65,536 bytes y el overflow límite sigue rechazado.

El oráculo calcula padding mediante residuo, comprueba la suma antes de aplicarla y valida por separado las dos
propiedades escalares de plataforma. Es iterativo y no usa recursión.

## Evidencia

- Gate `CF-TGT-021`: 8/8 en Debug (1.47 s) y Release (0.43 s).
- Suite nueva completa: Debug 39/39 en 11.86 s; Release 39/39 en 3.48 s.
- Controlador Python: 51/51; fuentes Python: 60/60 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos.
- Manifiesto: 298 casos, 0 duplicados y 8 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-161` | padding mínimo y propiedades |
| `TF-TGT-162` | agregado representativo con mínimo |
| `TF-TGT-163` | ausencia de estado externo |
| `TF-TGT-164` | overflow de padding final |
| `TF-TGT-165` | vacío, exactitud y límite de alineación |
| `TF-TGT-166` | matriz de 120 variantes |
| `TF-TGT-167` | determinismo y orden de invocación |
| `TF-TGT-168` | escala y overflow límite |

Diagramas: no aplica; el contrato es una operación escalar de redondeo al cierre.
