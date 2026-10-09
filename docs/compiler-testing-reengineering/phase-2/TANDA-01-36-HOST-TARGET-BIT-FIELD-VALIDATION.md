# Tanda 01.36 — Validación de solicitudes bit-field de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CF-TGT-017`, casos `TF-TGT-129..136` |
| Producto | Observar la validación bit-field de `layoutStruct`; no modificar |
| Excluido | Packing secuencial, barreras de ancho cero y unions bit-field |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- la solicitud mínima 1/1 con width 1 produce un bit-field exacto en offset y bit offset cero;
- unidades de 2, 4 y 8 bytes aceptan anchos parciales y el ancho total exacto;
- una instancia nueva funciona sin metadata ni estado externo;
- offset explícito, mínimo local, flexible array y alineación cero se rechazan;
- ancho cero anónimo se acepta, ancho cero nombrado se rechaza, el máximo exacto se acepta y máximo más uno se rechaza;
- un tamaño de unidad que desbordaría la conversión bytes-a-bits se rechaza antes de multiplicar;
- 256 combinaciones de tamaño, ancho y cuatro flags coinciden con una tabla de verdad independiente;
- ocho fixtures conservan resultado exacto ante repetición y cambio del orden de invocación;
- 65,536 solicitudes generadas coinciden con la tabla de verdad y el overflow límite permanece rechazado.

El oráculo no replica el packing posterior: decide admisión mediante una tabla de verdad y sólo para solicitudes
aceptadas de un campo calcula el resultado exacto. Es iterativo, de costo constante por solicitud y no usa recursión.

## Evidencia

- Gate `CF-TGT-017`: 8/8 en Debug (1.39 s) y Release (0.41 s).
- Suite nueva completa: Debug 35/35 en 9.71 s; Release 35/35 en 2.96 s.
- Controlador Python: 51/51; fuentes Python: 56/56 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos después de hacer explícito el acceso protegido al `optional`.
- Manifiesto: 266 casos, 0 duplicados y 8 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes protegidos conservaron exactamente sus hashes SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-129` | bit-field mínimo |
| `TF-TGT-130` | unidades y anchos representativos |
| `TF-TGT-131` | ausencia de estado externo |
| `TF-TGT-132` | flags incompatibles y alineación cero |
| `TF-TGT-133` | anchos y tamaño de unidad límite |
| `TF-TGT-134` | tabla de verdad de 256 combinaciones |
| `TF-TGT-135` | determinismo y orden de invocación |
| `TF-TGT-136` | escala y overflow bytes-a-bits |

Diagramas: no aplica; el contrato es una tabla de validación sin transición de estado entre solicitudes.
