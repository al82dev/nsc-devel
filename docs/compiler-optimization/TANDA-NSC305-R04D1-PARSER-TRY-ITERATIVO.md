# NSC 3.0.5 — R04D1: `try`/`catch`/`finally` iterativos

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`, `4d000d0b8`.
- **Objetivo exacto:** eliminar los frames C++ retenidos entre un `try` y sus bloques protected,
  catch y finally mediante continuaciones heap-backed.
- **Naturaleza:** refactor interno no funcional del parser.
- **Último estado focal correcto:** R04C cerrado; `nsc_project_method_body_model_tests` pasa 95/95.
- **Rango evaluado:** `4d000d0b8..worktree`.
- **Entrada exacta:** `try` anidados a través del protected block, de cuerpos catch o de finally.
- **Operación exacta:** `parseTryStatement -> parseBlock -> parseStatement -> parseTryStatement` y
  `parseCatchClause -> parseBlock` conservan un frame del constructo exterior por nivel.
- **Invariante:** protected block, orden de catches, filtro `when`, catch-all, finally, recovery,
  ranges y diagnostics conservan exactamente su forma 3.0.4.
- **Consumidor afectado:** parser de statements y modelos posteriores que consumen
  `TryStatement`/`CatchClause`.
- **Archivos de producto autorizados:** exclusivamente `src/Parser/Parser.cpp`.
- **Evidencia autorizada:** `tests/unit/ProjectMethodBodyModelTests.cpp`, este expediente y
  tracking externo.
- **Símbolos autorizados:** `Parser::StructuredControlParser`, wrappers
  `Parser::parseTryStatement`/`parseCatchClause` y helpers privados locales indispensables.
- **Archivos y subsistemas protegidos:** headers públicos, AST, lexer, expresiones/filtros Pratt,
  semántica, HIR, MIR, LLVM, runtime, Driver, metadata, Builder, fixtures y expectativas.
- **Pruebas inmutables:** 95 comprobaciones focales anteriores, TP-PAR-415…427 y pruebas Stage45;
  no se cambia ninguna expectativa.
- **Prohibido:** modificar `using` o `defer` (R04D2/D3), expresiones del filtro, cleanup semántico,
  agregar límite de profundidad, baseline, suite completa, deploy, instalación, push, pull o fetch.
- **Criterio exacto de terminación:** contrafactual directo profundo cambia de stack overflow a
  PASS, forma/recovery/orden se preservan, foco previo permanece verde, `nsc` construye, diff y
  commit local aislado quedan auditados.

## Presupuesto obligatorio de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | parser `try`/`catch`/`finally` |
| Producto | `src/Parser/Parser.cpp` |
| Prueba | `tests/unit/ProjectMethodBodyModelTests.cpp` |
| Documentación | este expediente y tracking externo |
| Configuración | `build-test-infrastructure` existente |
| Perfil | focal; sin baseline |

## Prueba contrafactual

`parseTryStatement` y `parseCatchClause` participan directamente: sus frames conservan el nodo,
begin index y clauses mientras un bloque hijo puede volver a entrar a otro `try`. Los filtros
`when` pertenecen a R03 y permanecen protegidos.

## Diseño autorizado

La máquina estructural recibe un estado heap-backed por `try` y continuaciones separadas para:

1. protected block;
2. header y body de cada catch en orden fuente;
3. finally opcional;
4. diagnóstico de handler ausente y cierre de range.

El header catch conserva type, nombre, catch-all y filtro antes de agendar su bloque. No se cambia
ningún nodo público ni el lowering posterior.

## Estado

Terminado.

## Evidencia roja previa al producto

La prueba directa construyó y parseó dos fuentes independientes con 1,024 niveles:

1. `try { ... } finally { ... }` anidados por el protected block;
2. `try { } catch { ... }` anidados por el body del catch.

Contra la revisión base `4d000d0b8`, el proceso terminó con `0xC00000FD`
(`-1073741571`, stack overflow). La reproducción no pasó por method bodies, HIR, MIR ni LLVM:
el fallo quedó aislado en `Lexer -> Parser::parseCompilationUnit`.

## Implementación

`Parser::StructuredControlParser` ahora conserva en vectores heap-backed el estado de cada
`try` y de cada catch standalone. Las continuaciones explícitas separan:

- finalización del protected block;
- lectura del header catch y finalización de su body;
- reanudación para el catch siguiente;
- finalización del bloque `finally`;
- diagnóstico y cierre del range del `try`.

`Parser::parseTryStatement` y `Parser::parseCatchClause` quedaron como wrappers de esa máquina.
No se modificó el parsing Pratt del filtro `when`, el AST público ni ningún consumidor posterior.

## Validación focal

- Construcción con `/W4 /WX`: `nsc_project_method_body_model_tests`, `nsc_parser_tests` y `nsc`.
- Contrato de host runtime estático: PASS.
- `nsc_project_method_body_model_tests`: `PASS: 98 checks`.
- Profundidad directa: 1,024 niveles de `try/finally` y catch body, PASS sin stack overflow.
- Contratos: slots protected/finally, orden de catches, tipo/nombre, filtro, catch-all y diagnóstico
  exacto de handler ausente, PASS.
- `nsc_stage45_exceptions_syntax_tests`: `PASS: 33 checks`.
- `nsc_parser_tests`: 22/23; permanece únicamente el rojo ajeno ya conocido de ternario. No se
  contó como aprobación ni se modificó su expectativa.
- `git diff --check`: limpio.
- Líneas agregadas mayores de 120 caracteres: ninguna.

No se ejecutaron baseline, suite completa, deploy, instalación ni pruebas de consumidores.
