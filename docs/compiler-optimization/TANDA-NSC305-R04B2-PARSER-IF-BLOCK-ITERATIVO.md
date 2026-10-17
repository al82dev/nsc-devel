# NSC 3.0.5 — R04B2: cuerpos `if` y bloques iterativos

## SCOPE LOCK

- **Repositorio/rama:** `A:\nsc-devel`, `development`.
- **Hipótesis única:** aunque R04B1 eliminó la autorrecursión de `else if`, un `if` dentro del
  bloque de otro `if` todavía reentra por
  `parseIfStatement -> parseEmbeddedStatementBlock -> parseBlock -> parseStatement`.
- **Archivos autorizados:** `include/nativesharp/Parser/Parser.h`, `src/Parser/Parser.cpp`, la prueba
  focal existente `tests/unit/ProjectMethodBodyModelTests.cpp`, este expediente y tracking externo.
- **Contratos congelados:** AST y ranges, orden fuente, dangling-else, bloques vacíos, statements
  embebidos, condition declarations, expansión `defer-registration`, inline de pattern
  condicional, recovery/progreso, diagnostics, sintaxis, semántica, API, ABI, NSMETA y CLI 3.0.4.
- **Prohibido:** aumentar stack, limitar profundidad, reinterpretar `else`, cambiar otras familias
  estructuradas, baseline, suite completa, deploy o instalación.
- **Gate:** 256 cuerpos `if` anidados y 512 ramas `else if` pasan dentro del foco de 87 checks;
  `nsc` construye; la ruta ordinaria de cuerpos `if` no reentra al stack C++ por
  `parseBlock`/`parseStatement`.
- **Límite honesto:** `while`/`do`/`for`, conditional-pattern, switch/match y cleanup/exceptions
  permanecen en subítems posteriores.

## Diseño

`parseIfStatement` ejecutará una pila de tareas con valores AST:

1. `ParseIf` consume sólo header/condición.
2. `ParseEmbedded` conserva bloque explícito, `;` vacío o wrapping de un statement.
3. `ParseBlock`/`ContinueBlock` recorren siblings con un frame heap-backed y el destino efectivo
   que requiere `defer`.
4. Un `if` ordinario encontrado dentro del bloque apila otra tarea `ParseIf`; no llama al parser de
   statements recursivamente.
5. `FinishThen` inspecciona `else` después de terminar el cuerpo; `FinishElse` materializa con el
   mismo `lowerIfStatement` usado por R04B1.

## Estado

R04B2 terminado el 2026-10-17.

## Resultado

- `parseIfHeader` ya no entra al cuerpo.
- `parseIfStatement` usa tareas explícitas para headers, embedded statements, bloques, anexos y
  finalización `then`/`else`.
- Los bloques conservan un `destination` mutable y reutilizan `appendParsedStatement`, por lo que
  `defer-registration` y `conditional-pattern-inline` mantienen una sola implementación.
- El `else` se inspecciona únicamente después de completar el `then`, preservando la asociación al
  `if` abierto más cercano.
- Prueba focal: `PASS: 87 checks`, incluidos 256 cuerpos `if`, 512 ramas `else if` y 256 `foreach`.
- Construcción target `nsc`: PASS; contrato estático de host runtime: PASS.
- `git diff --check`: PASS.

No se ejecutaron baseline, suite completa, deploy ni instalación. R04B continúa con loops.
