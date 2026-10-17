# NSC 3.0.5 — H02S8: blocks HIR iterativos

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la reentrada `lowerBlock` → `lowerStatement(Block)` → `lowerBlock`. |
| Algoritmo | DFS de frames de block con cursor de statement y resultado reanudable. |
| Símbolos | `ProjectOopHirModel::lowerBlock` y contrafactual focal directo. |
| Incluido | Blocks anidados, orden fuente, regiones cleanup, primer fallo y forma HIR. |
| Excluido | Control flow no-block, expresiones, calls, construcción, async, HIR base y MIR. |
| Terminación | Rojo/verde profundo, regresiones HIR, build de `nsc`, análisis y tracking. |

## Cadena causal previa al cambio

Cada `BlockStatement` hijo era enviado a `lowerStatement`, que inmediatamente volvía a entrar en
`lowerBlock`. Una cadena de blocks sintácticamente válida consumía una llamada del proceso por nivel
aunque cada frame sólo necesita conservar un cursor, su nodo HIR y la profundidad cleanup de entrada.

## Presupuesto de cambio

- Repositorio: sólo `A:\nsc-devel`.
- Producción: `src/Frontend/HIR/HirFunctionLowerer.cpp`.
- Prueba: acceso focal y contrafactual en `ProjectOopLoweringTests.cpp`.
- Contratos protegidos: orden de statements, cleanup, errores, forma HIR, API, ABI y NSMETA.
- Validación: sólo build y pruebas focales O0; sin baseline, batería completa, deploy o instalación.

## Evidencia

### Contrafactual rojo

El foco `hir-block-depth` construyó 32,768 `BlockStatement` anidados en almacenamiento plano y
llamó directamente a `lowerBlock`. Con la reentrada anterior, el proceso terminó con
`-1073741571` (`0xC00000FD`) antes de producir el root HIR.

### Implementación

- Cada block usa un frame con sintaxis, nodo HIR parcial, profundidad cleanup de entrada y cursor
  del siguiente statement.
- Un hijo `BlockStatement` apila un frame directamente; al completarse se anexa al padre y éste
  reanuda el statement siguiente.
- Los statements que no son blocks continúan pasando por `lowerStatement` sin cambiar su dispatch.
- Cada frame restaura su profundidad cleanup al cerrarse. En el primer fallo se restaura la
  profundidad del root y se conserva el diagnóstico emitido por el statement original.
- El orden de hijos y la forma `Block` por nivel son idénticos al recorrido recursivo previo.

### Resultado verde y paridad

- `hir-block-depth`: `PASS 2/2`, exit `0`.
- El foco comprueba 32,768 niveles, el orden estructural y el leaf vacío.
- La unidad OOP general quedó 134/136; los dos únicos fallos son enlaces porque `nslink.exe` no
  está junto a `nsc.exe` ni en el `PATH`, y no se declaran aprobados.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- El target focal y `nsc` compilaron con `/W4 /WX`; cero diagnósticos nuevos.
- El contrato de runtime estático de `nsc.exe` pasó.
- `git diff --check` se ejecuta como gate final antes del commit.

No se ejecutaron baseline, batería completa, deploy, instalación ni consumidores externos.

## Residuo H02 confirmado

La arista de blocks puros ya no reentra al proceso. El SCC OOP todavía conecta statements de
control con expressions, calls, construction, conversion, aggregate, await, destruction,
exceptions y coroutines. También permanecen las rutas productivas del HIR base en
`hir::HirBuilder`, `hir::HirVerifier` y `hir::HirDumper`.

El siguiente corte se seleccionará por arista causal y contrafactual profundo; no se declarará H02
cerrado mientras la reauditoría encuentre cualquiera de esas rutas.
