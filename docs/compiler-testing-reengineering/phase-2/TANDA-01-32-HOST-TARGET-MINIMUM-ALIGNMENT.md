# Tanda 01.32 — Alineación mínima de agregados y campos

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Target` |
| Contrato | `CF-TGT-013`, alineación mínima agregada y por campo |
| Casos implementados | `TF-TGT-097..104` |
| Código de producto | Incluir y observar el overload general de `layoutStruct`; no modificar |
| Uso puntual de offset explícito | Sólo validar el rechazo de un offset desalineado exigido por el contrato |
| Superficie excluida | Colocación explícita general, union, flexible arrays y bit-fields |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

La tanda cierra los ocho casos diseñados para `CF-TGT-013`, incluido el caso `NIGHTLY`. Comprueba que la
alineación mínima del campo eleva la alineación efectiva después de aplicar pack y que la mínima agregada eleva
la alineación y el padding final sin sustituir los requisitos de cada campo.

## Contratos ejercitados

- un byte con mínimo de campo 2 produce layout 2/2 y offset cero;
- pack 2, un segundo campo con mínimo 8 y mínimo agregado 16 producen offsets 0 y 8 y layout 16/16;
- una instancia nueva procesa mínimos de campo y agregado sin estado externo;
- mínimo de campo cero, 3 o `uint64_t::max()`, offset explícito 2 con mínimo 4 y alineación cercana al máximo
  producen `nullopt`;
- `2^63`, la mayor potencia de dos representable, permanece exacta tanto como mínimo agregado como por campo;
- 150 combinaciones de pack, mínimo de campo y mínimo agregado coinciden con un oráculo independiente;
- repetición no cambia offsets, alineaciones, tamaño ni vector de campos;
- 65,536 campos con mínimos cíclicos y mínimo agregado 64 coinciden con el oráculo y el caso cercano al máximo
  permanece rechazado.

## Oráculo independiente

El oráculo alinea mediante residuo y padding, valida cada potencia de dos, calcula
`max(min(natural, pack), fieldMinimum)` y comprueba cada suma antes de realizarla. La comparación incluye tamaño,
alineación y todos los campos. Es iterativo, `O(F)` en tiempo y `O(F)` por el resultado; no usa recursión.

## Evidencia de validación

- Gate dirigido `CF-TGT-013`: 8/8 aprobado en Debug y Release.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y suite nueva 31/31 aprobada en paralelo en 7.56 s.
- Release: build con warnings como errores y suite nueva 31/31 aprobada en 11.53 s.
- `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos.
- Compilación en memoria de los 52 archivos Python del controlador: 52/52 aprobada.
- Manifiesto: 234 casos válidos, 0 IDs duplicados y 8 casos de `CF-TGT-013`.
- Dependencias dinámicas de ambos ejecutables: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Diferencial de producto bajo `src` e `include/nativesharp`: 0 archivos.
- Los seis archivos preexistentes protegidos conservan sus SHA-256.
- No se ejecutaron NSC, NSLink, las 684 pruebas históricas, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-097` | mínimo por campo |
| `TF-TGT-098` | composición con pack y mínimo agregado |
| `TF-TGT-099` | ausencia de estado externo |
| `TF-TGT-100` | valores inválidos, desalineación y overflow |
| `TF-TGT-101` | mayor potencia de dos representable |
| `TF-TGT-102` | matriz de 150 combinaciones |
| `TF-TGT-103` | determinismo |
| `TF-TGT-104` | escala y overflow límite |

Diagramas: no aplica. El contrato es una transformación secuencial sin estado.
