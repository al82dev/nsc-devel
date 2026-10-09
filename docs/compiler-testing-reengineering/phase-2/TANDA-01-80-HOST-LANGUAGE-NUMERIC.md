# Tanda 01.80 - Host.Language: semántica numérica

Fecha: 2026-10-09

## Alcance implementado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-016` |
| Casos implementados | 24/25: `TL-LANG-192..200`, `TL-LANG-202..216` |
| Caso bloqueado | `TL-LANG-201` |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | PARCIAL POR DEFECTO DE PRODUCTO |

La tanda comprueba literales enteros, políticas de overflow, shifts, conversiones
numéricas, contratos floating point, strong numerics, memoria constante, unions y
evaluación constante. Los casos invocan las interfaces públicas vigentes y no
reimplementan el comportamiento de producción como sustituto.

## Contratos comprobados

| Casos | Comportamiento verificado |
|---|---|
| `TL-LANG-192..198` | Límites, parsing y resolución exacta de literales enteros válidos e inválidos. |
| `TL-LANG-199..200` | Políticas de overflow y aritmética add/sub/mul en anchos de 1 a 64 bits. |
| `TL-LANG-202..203` | Shifts de borde y reconstrucción signed bit-exacta. |
| `TL-LANG-204..206` | Matriz de conversiones y contratos Strict/Precise/Fast. |
| `TL-LANG-207..210` | Identidad, plan, errores y operadores de strong numerics. |
| `TL-LANG-211..214` | Provenance, offset, dereference, deallocation y active member constante. |
| `TL-LANG-215..216` | Paridad runtime/consteval, budget de loops y barrida signed i8. |

## Bloqueo confirmado

### `TL-LANG-201`: remainder no existe en la interfaz pública

El diseño exige comprobar división y remainder por cero, además de signed min/-1.
`IntegerArithmeticOperation` sólo publica `Add`, `Subtract`, `Multiply`, `Divide`,
`ShiftLeft` y `ShiftRight`. No hay operación `Remainder`, por lo que el contrato no
puede ejercitarse completo desde la API pública.

- `BLOQUEADO_POR`: ausencia de `Remainder` en el modelo y evaluador de aritmética entera.
- La parte de división no convierte en cumplido un caso cuyo alcance también exige remainder.
- Resolverlo requiere ampliar producto; la Fase 2 prohíbe ese cambio.
- No se inventó una operación, no se duplicó un oráculo y no se congeló la ausencia como éxito.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido realizable: Debug 24/24 en 14.23 s; Release 24/24 en 1.15 s.
- Suite nueva completa: Debug 76/76 en 404.56 s; Release 76/76 en 40.93 s.
- Controlador Python: 51/51 en 0.641 s.
- Fuentes Python de la suite: 97/97 parseadas en memoria.
- Manifiesto: 654 casos, 0 duplicados y 24 casos implementados de `CL-LANG-016`.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en los dos módulos nuevos.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; código y configuración modificados con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

`CL-LANG-016` queda con 24 casos implementados y verificados y un bloqueo de producto
documentado. La Fase 2 puede continuar con `CL-LANG-017`, pero no declarar esta ola
completa mientras `TL-LANG-201` siga bloqueado.

## Adenda de test plan — 2026-10-09

Remainder no forma parte de `IntegerArithmeticOperation`; exigirlo fue un error
del plan, no un hueco del compilador. `TL-LANG-201` conserva las capacidades
vigentes de división, división por cero y `signed min / -1` bajo las políticas
actuales. El caso queda `PLAN CORREGIDO — IMPLEMENTABLE` sin cambios de producto.

El caso fue implementado posteriormente y pasó su ejecución exclusiva. El
contrato queda en 25/25 casos implementados y verificados.
