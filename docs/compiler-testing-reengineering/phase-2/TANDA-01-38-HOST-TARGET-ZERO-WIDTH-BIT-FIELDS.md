# Tanda 01.38 — Barreras bit-field de ancho cero de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CF-TGT-019`, casos `TF-TGT-145..152` |
| Producto | Observar barreras bit-field anónimas de width cero; no modificar |
| Excluido | Packing general no cero y unions bit-field |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- una barrera líder queda en offset cero, tamaño cero y no eleva la alineación agregada;
- una barrera después de una unidad activa alinea el cursor, eleva extent/alineación y cierra la unidad;
- el bit-field posterior abre una unidad nueva desde la frontera calculada;
- una instancia nueva funciona sin metadata ni estado externo;
- un campo nombrado de width cero se rechaza;
- barreras líderes, líderes duplicadas, intermedias y finales coinciden con el oráculo;
- 60 combinaciones de posición, alineación base y `pack` coinciden con un oráculo independiente;
- cinco fixtures conservan el layout exacto ante repetición y cambio del orden de invocación;
- 32,768 pares campo/barrera producen 65,536 registros y tamaño 32,768 sin recursión.

El oráculo mantiene la unidad activa y modela de forma separada la transición de barrera: sólo una unidad activa
obliga a alinear, actualizar extent y elevar alineación. Es iterativo, `O(F)` tiempo y `O(F)` memoria.

## Evidencia

- Gate `CF-TGT-019`: 8/8 en Debug (1.36 s) y Release (0.42 s).
- Suite nueva completa: Debug 37/37 en 10.88 s; Release 37/37 en 3.12 s.
- Controlador Python: 51/51; fuentes Python: 58/58 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos después de tipar la posición con enum fuerte.
- Manifiesto: 282 casos, 0 duplicados y 8 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-145` | barrera líder mínima |
| `TF-TGT-146` | cierre y reapertura de allocation unit |
| `TF-TGT-147` | ausencia de estado externo |
| `TF-TGT-148` | width cero nombrado inválido |
| `TF-TGT-149` | posiciones y barreras duplicadas |
| `TF-TGT-150` | matriz de 60 variantes |
| `TF-TGT-151` | determinismo y orden de invocación |
| `TF-TGT-152` | escala de 65,536 registros |

Diagramas: no aplica; el contrato se expresa con una transición lineal de unidad activa a cerrada.
