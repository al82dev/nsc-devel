# NSC 3.0.5 — R04D2: `using` iterativo

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`, `ff384b003`.
- **Objetivo exacto:** eliminar el frame C++ retenido por cada `using` mientras se parsea su body.
- **Naturaleza:** refactor interno no funcional del parser.
- **Rango evaluado:** `ff384b003..worktree`.
- **Entrada exacta:** statements `using` anidados a través de su bloque body.
- **Operación exacta:** `parseUsingStatement -> parseBlock -> parseStatement ->
  parseUsingStatement` conserva un frame exterior por nivel.
- **Invariante:** tipo, nombre, initializer evaluable una vez, body, ranges, recovery y diagnostics
  conservan exactamente su forma 3.0.4.
- **Consumidor afectado:** parser de statements y modelos posteriores que consumen
  `UsingStatement`.
- **Archivos de producto autorizados:** exclusivamente `src/Parser/Parser.cpp`.
- **Evidencia autorizada:** `tests/unit/ProjectMethodBodyModelTests.cpp`, este expediente y
  tracking externo.
- **Símbolos autorizados:** `Parser::StructuredControlParser`, wrapper
  `Parser::parseUsingStatement` y helpers privados locales indispensables.
- **Archivos y subsistemas protegidos:** headers públicos, AST, lexer, expresiones Pratt,
  semántica cleanup, HIR, MIR, LLVM, runtime, Driver, metadata, Builder y fixtures existentes.
- **Pruebas inmutables:** foco previo 98/98, TP-PAR-425/426 y Stage45; no se cambia ninguna
  expectativa.
- **Prohibido:** modificar `defer` (R04D3), initializer expressions, lowering cleanup, agregar
  límite de profundidad, baseline, suite completa, deploy, instalación, push, pull o fetch.
- **Criterio exacto de terminación:** contrafactual directo profundo cambia de stack overflow a
  PASS, forma y recovery se preservan, foco previo permanece verde, `nsc` construye, diff y commit
  local aislado quedan auditados.

## Presupuesto obligatorio de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | parser de `using` |
| Producto | `src/Parser/Parser.cpp` |
| Prueba | `tests/unit/ProjectMethodBodyModelTests.cpp` |
| Documentación | este expediente y tracking externo |
| Configuración | `build-test-infrastructure` existente |
| Perfil | focal; sin baseline |

## Diseño autorizado

La máquina estructural conservará el begin index y el nodo `UsingStatement` en estado
heap-backed. El header se leerá una sola vez antes de agendar el body y una continuación asignará
el bloque final sin retener el frame de `parseUsingStatement`. El initializer seguirá en la ruta
Pratt existente y queda fuera de esta tanda.

## Estado

Terminado.

## Evidencia roja previa al producto

El contrafactual directo construyó una unidad con 1,024 `using` anidados y la entregó a
`Lexer -> Parser::parseCompilationUnit`. Contra `ff384b003`, el proceso terminó con
`0xC00000FD` (`-1073741571`, stack overflow), antes de method bodies, HIR, MIR o LLVM.

## Implementación

`Parser::StructuredControlParser` reconoce ahora `KwUsing`. El header conserva exactamente la
ruta anterior para tipo, nombre, `=`, initializer y `)`. El nodo y su begin index quedan en
`UsingParseState`; `FinishUsing` recibe el bloque ya parseado, asigna `second()` y cierra el range.

`Parser::parseUsingStatement` quedó como wrapper de la máquina estructural. El initializer sigue
pasando por `parseExpression`, sin cambios de precedencia, recuperación ni evaluación posterior.

## Validación focal

- Construcción con `/W4 /WX`: `nsc_project_method_body_model_tests`, `nsc_parser_tests`,
  `nsc_stage45_exceptions_syntax_tests` y `nsc`.
- Contrato de host runtime estático: PASS.
- `nsc_project_method_body_model_tests`: `PASS: 100 checks`.
- Profundidad directa: se contaron los 1,024 nodos `UsingStatement`, PASS sin stack overflow.
- Recovery equivalente a TP-PAR-426: un cleanup malformado conserva el `return` siguiente, PASS.
- `nsc_stage45_exceptions_syntax_tests`: `PASS: 33 checks`; conserva tipo, nombre, initializer,
  body y representación del AST.
- `nsc_parser_tests`: 22/23; permanece sólo el rojo ajeno ya conocido de ternario.
- El intento de reconstruir `nsc_component_tests` no llegó a ejecutar TP-PAR-425/426: el linker
  encontró símbolos faltantes preexistentes de la partición `ProjectMethodBodyModel`. No se contó
  como PASS ni se amplió esta tanda para corregir infraestructura ajena.
- `git diff --check`: limpio.
- Líneas agregadas mayores de 120 caracteres: ninguna.

No se ejecutaron baseline, suite completa, deploy, instalación ni pruebas de consumidores.
