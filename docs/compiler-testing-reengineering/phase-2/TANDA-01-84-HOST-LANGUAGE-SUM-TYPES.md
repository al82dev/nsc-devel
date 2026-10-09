# Tanda 01.84 - Host.Language: sum types y patterns

Fecha: 2026-10-09

## Alcance implementado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-020` |
| Casos implementados | 9/10: `TL-LANG-261`, `TL-LANG-263..270` |
| Casos bloqueados | `TL-LANG-262` |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | PARCIAL POR BLOQUEO DE PRODUCTO |

La tanda comprueba el layout de variantes, la compatibilidad de bindings en
OR-patterns, la exhaustividad de matches cerrados, `let-else`, las lecturas de
uniones C y la identidad estable de metadata. Los límites de tag incluyen las
transiciones efectivas de 256 a 257 y de 65,536 a 65,537 casos, además de los
valores indicados explícitamente por el plan.

## Contratos comprobados

| Casos | Comportamiento verificado |
|---|---|
| `TL-LANG-261` | Tags mínimos de 1/2/4 bytes y layout exacto para 1, 255, 256, 257, 65,535, 65,536 y 65,537 casos. |
| `TL-LANG-263` | Offset, tamaño, alineación, padding y layout histórico exactos con payloads mixtos. |
| `TL-LANG-264` | Bindings idénticos en distinto orden son compatibles y limpian el error previo. |
| `TL-LANG-265` | Diferencias de count, name, type y mode se rechazan con el diagnóstico exacto. |
| `TL-LANG-266` | Un duplicado izquierdo conserva el primer binding insertado mediante `emplace`. |
| `TL-LANG-267` | Cobertura exacta, missing cases, wildcard, guarded arms y brazos inalcanzables. |
| `TL-LANG-268` | La garantía de inicialización coincide con `elseDiverges` para ambos valores. |
| `TL-LANG-269` | Una unión C se puede leer exactamente cuando existe proof o contexto unsafe. |
| `TL-LANG-270` | Identidad determinista para 0/1/N casos; tamaño y alineación no la modifican. |

## Bloqueo confirmado

`TL-LANG-262` exige que nombres vacíos, alineaciones inválidas y cualquier
overflow produzcan `nullopt`. Las dos primeras familias sí tienen una frontera
segura, pero la implementación vigente no comprueba el overflow del padding
final. Con un caso no vacío, alineación 8 y payload de
`SIZE_MAX - 8`, `payloadOffset + payloadSize` es exactamente `SIZE_MAX`; el
chequeo previo no lo rechaza y `alignUp(SIZE_MAX, 8)` desborda internamente,
produciendo un layout con tamaño cero en lugar de `nullopt`.

Marcar el caso como aprobado ocultaría una parte obligatoria de su matriz. La
Fase 2 no puede modificar producto, por lo que `TL-LANG-262` permanece bloqueado
hasta que `computeWindowsX64VariantLayout` tenga una frontera de aritmética
comprobada también para el redondeo final.

## Evidencia reproducible

- Builds reales de configuración única: Release y Debug correctos, con warnings como errores.
- Gate dirigido Debug: 9/9 en 7.29 s; gate Release dentro de la regresión final: 9/9 en 0.57 s.
- Suite nueva completa final: Debug 80/80 en 507.88 s; Release 80/80 en 46.51 s.
- Controlador Python: 51/51 en 0.767 s.
- Fuentes Python de la suite: 101/101 parseadas en memoria.
- Manifiesto: 705 casos, 0 duplicados y 9 casos implementados de `CL-LANG-020`.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en los dos módulos nuevos.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; código y configuración modificados con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

`CL-LANG-020` queda parcialmente cerrado con 9 casos implementados y verificados.
Su único pendiente, `TL-LANG-262`, está identificado como bloqueo de producto y
no como deuda oculta de la suite. La Fase 2 puede continuar con `CL-LANG-021`
conservando este bloqueo explícito en el cursor.
