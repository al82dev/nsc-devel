# NSC 3.0.5 — R04D3: `defer` iterativo

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`, `a625413e5`.
- **Objetivo exacto:** eliminar el frame C++ retenido por cada `defer` mientras se parsea su body.
- **Naturaleza:** refactor interno no funcional del parser.
- **Rango evaluado:** `a625413e5..worktree`.
- **Entrada exacta:** statements `defer` anidados a través de su bloque body.
- **Operación exacta:** `parseDeferStatement -> parseBlock -> parseStatement ->
  parseDeferStatement` conserva un frame exterior por nivel.
- **Invariantes:** captura `ref`/`move`/`this`, orden y diagnósticos, nombres temporales, frontera de
  captura, registration, protected block, cleanup, orden LIFO, destination del bloque, ranges y
  recovery conservan exactamente su forma 3.0.4.
- **Consumidor afectado:** parser de statements y modelos posteriores que consumen el lowering AST
  `defer-registration`/`defer-lowered`.
- **Archivos de producto autorizados:** exclusivamente `src/Parser/Parser.cpp`.
- **Evidencia autorizada:** `tests/unit/ProjectMethodBodyModelTests.cpp`, este expediente y
  tracking externo.
- **Símbolos autorizados:** `Parser::StructuredControlParser`, wrapper
  `Parser::parseDeferStatement` y helpers privados locales indispensables.
- **Archivos y subsistemas protegidos:** headers públicos, AST, lexer, expresiones Pratt,
  semántica cleanup, HIR, MIR, LLVM, runtime, Driver, metadata, Builder y fixtures existentes.
- **Pruebas inmutables:** foco previo 100/100 y TP-PAR-412…417; no se cambia ninguna expectativa.
- **Prohibido:** rediseñar defer o cleanup, cambiar captures o nombres generados, agregar límite de
  profundidad, baseline, suite completa, deploy, instalación, push, pull o fetch.
- **Criterio exacto de terminación:** contrafactual directo profundo cambia de stack overflow a
  PASS, contratos de captura/LIFO/recovery se preservan, foco previo permanece verde, `nsc`
  construye, diff y commit local aislado quedan auditados.

## Presupuesto obligatorio de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | parser/lowering AST de `defer` |
| Producto | `src/Parser/Parser.cpp` |
| Prueba | `tests/unit/ProjectMethodBodyModelTests.cpp` |
| Documentación | este expediente y tracking externo |
| Configuración | `build-test-infrastructure` existente |
| Perfil | focal; sin baseline |

## Diseño autorizado

La máquina estructural conservará begin index, defer range y captures en estado heap-backed. El
header se leerá una vez y una continuación recibirá el bloque terminado. Sólo entonces ejecutará
el lowering AST existente. Esta secuencia preserva que los defer interiores reciban sus ordinales
antes que los exteriores y que `appendParsedStatement` avance el destination exacto del bloque.

## Estado

Terminado.

## Evidencia roja previa al producto

El contrafactual directo construyó una unidad con 1,024 cuerpos `defer [] { ... }` anidados y la
entregó a `Lexer -> Parser::parseCompilationUnit`. Contra `a625413e5`, el proceso terminó con
`0xC00000FD` (`-1073741571`, stack overflow), antes de method bodies, HIR, MIR o LLVM.

## Implementación

`Parser::StructuredControlParser` reconoce ahora el keyword contextual `defer`. El estado
heap-backed conserva begin index, defer range y captures en orden fuente. `FinishDefer` recibe el
body terminado y ejecuta sin cambios contractuales el lowering AST anterior.

El lowering ocurre después del body, igual que antes. Por ello un defer interior consume primero
su ordinal, los temporales `__nsc_defer_capture_*` conservan orden y nombres, y
`appendParsedStatement` sigue avanzando el destination del bloque a través de cada protected
region. `Parser::parseDeferStatement` quedó como wrapper de la máquina estructural.

## Validación focal

- Construcción con `/W4 /WX`: `nsc_project_method_body_model_tests`, `nsc_parser_tests`,
  `nsc_stage45_exceptions_syntax_tests` y `nsc`.
- Contrato de host runtime estático: PASS.
- `nsc_project_method_body_model_tests`: `PASS: 105 checks`.
- Profundidad directa: se contaron los 1,024 nodos `defer-lowered`, PASS sin stack overflow.
- Equivalentes focales TP-PAR-412…417: captureless, `ref`/`move`, orden de captures y frontera de
  nombres, nesting LIFO, límite lexical y recovery con `return`, PASS.
- `nsc_stage45_exceptions_syntax_tests`: `PASS: 33 checks`.
- `nsc_parser_tests`: 22/23; permanece sólo el rojo ajeno ya conocido de ternario.
- El runner `nsc_component_tests` no fue contado: su build focal sigue bloqueado por símbolos
  faltantes preexistentes de la partición `ProjectMethodBodyModel`, documentados en R04D2.
- `git diff --check`: limpio.
- Líneas agregadas mayores de 120 caracteres: ninguna.

No se ejecutaron baseline, suite completa, deploy, instalación ni pruebas de consumidores.
