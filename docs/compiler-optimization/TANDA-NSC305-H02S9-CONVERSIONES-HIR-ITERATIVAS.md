# NSC 3.0.5 — H02S9: conversiones HIR iterativas

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la reentrada por conversiones builtin/runtime en la máquina de expresiones. |
| Algoritmo | Frames de conversión dentro del scheduler H02C5 y reconstrucción postorden. |
| Símbolos | `ProjectOopHirModel::lowerExpression` y contrafactual focal directo. |
| Incluido | Cast ordinario, type-test, safe cast, checked runtime cast, tipo y provenance. |
| Excluido | Conversiones de usuario, calls, construcción, statements, HIR base y MIR. |
| Terminación | Rojo/verde profundo, regresiones HIR, build de `nsc`, análisis y tracking. |

## Cadena causal previa al cambio

Una conversión no atendida por H02C5 entraba a `lowerExpressionImpl`, luego a
`lowerConversionExpression` y finalmente volvía a `lowerExpression` para su operando. Una cadena
válida de casts consumía una llamada del proceso por nivel aunque su reconstrucción es unary y
postorden.

## Presupuesto de cambio

- Repositorio: sólo `A:\nsc-devel`.
- Producción: `src/Frontend/HIR/HirExpressionMachine.cpp`.
- Prueba: contrafactual en `ProjectOopLoweringTests.cpp` usando la costura H02C existente.
- Contratos protegidos: dispatch de conversiones de usuario, tipos, runtime binding, errores, HIR,
  API, ABI y NSMETA.
- Validación: sólo build y pruebas focales O0; sin baseline, batería completa, deploy o instalación.

## Evidencia

### Contrafactual rojo

El foco `hir-conversion-depth` construyó 32,768 `CastExpression` sobre un literal, con ownership AST
plano y type binding `builtin:int`. La ruta recursiva terminó con `-1073741571` (`0xC00000FD`)
antes de producir la hoja HIR.

### Implementación

- `Conversion` es ahora una continuación unary de la máquina iterativa H02C5.
- Type-test y safe-cast sólo entran a la máquina con runtime binding válido; si falta, la ruta
  anterior conserva el error antes de visitar el operando.
- `CheckedCast` mantiene precedencia sobre un posible call binding.
- El frame baja `first()` con `addressContext=false` y reconstruye después `RuntimeTypeTest`,
  `SafeReferenceCast`, `CheckedReferenceCast` o `Conversion`.
- Se conservan `resultType`, target ordinal, flag de interface, provenance y un hijo por wrapper.
- Una conversión de usuario no entra al frame: continúa en `lowerBoundCall` y se cerrará con calls.
- El fallback builtin no reclasifica la conversión ni rellena `node->conversion`, igual que antes.

### Resultado verde y paridad

- `hir-conversion-depth`: `PASS 6/6`, exit `0`.
- El foco comprueba 32,768 wrappers y la hoja literal.
- Pruebas superficiales comprueban type-test, safe cast y checked cast con ordinal/flag runtime.
- La falta de runtime binding conserva exactamente
  `Stage 39 runtime type operation lost its semantic binding` antes de visitar el operando.
- La unidad OOP general quedó 134/136; los dos únicos fallos son enlaces porque `nslink.exe` no
  está junto a `nsc.exe` ni en el `PATH`, y no se declaran aprobados.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- El target focal y `nsc` compilaron con `/W4 /WX`; cero diagnósticos nuevos.
- El contrato de runtime estático de `nsc.exe` pasó.
- `git diff --check` se ejecuta como gate final antes del commit.

No se ejecutaron baseline, batería completa, deploy, instalación ni consumidores externos.

## Residuo H02 confirmado

La arista directa conversion → operando ya no reentra al proceso para conversiones builtin/runtime.
Las conversiones de usuario conservan la arista `lowerBoundCall` → `lowerExpression`, que pertenece
al corte de calls. El SCC OOP y `hir::HirBuilder`/`HirVerifier`/`HirDumper` siguen abiertos.
