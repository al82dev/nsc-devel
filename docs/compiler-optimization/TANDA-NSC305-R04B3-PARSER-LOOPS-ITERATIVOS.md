# NSC 3.0.5 — R04B3: continuaciones iterativas de `while`/`do`/`for`

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`,
  `b0141acf026cd7647a74883ac0c0e40743bb6e9f`.
- **Naturaleza:** refactor interno no funcional del parser de statements.
- **Estado correcto de entrada:** `nsc.project-method-body-model` pasa en la revisión base con
  87 comprobaciones; R04A, R04B1 y R04B2 están cerrados y constituyen el diseño precedente.
- **Hipótesis causal única:** la máquina de R04B2 sólo intercepta `if` y bloques. Los cuerpos
  `while`, `do` y `for` vuelven a entrar por `parseStatement -> parse* -> parseBlock`, por lo que
  anidamientos homogéneos o cruzados conservan un frame C++ por nivel.
- **Entrada exacta:** cuerpos con llaves que anidan combinaciones de `if`, `while`, `do` y `for`.
- **Operación que falla estructuralmente:** continuar el cuerpo del control exterior mediante una
  llamada recursiva indirecta antes de poder materializar su AST.
- **Invariante:** la profundidad sintáctica de esas cuatro familias debe residir en memoria
  heap-backed y no en el stack del host.
- **Símbolos autorizados:** `Parser::parseIfStatement`, `Parser::parseWhileStatement`,
  `Parser::parseDoWhileStatement`, `Parser::parseForStatement`, sus headers/lowerings privados y
  una máquina privada compartida de continuaciones estructurales.
- **Archivos de producto autorizados:** `include/nativesharp/Parser/Parser.h` y
  `src/Parser/Parser.cpp`.
- **Evidencia autorizada:** `tests/unit/ProjectMethodBodyModelTests.cpp`, este expediente y el
  tracking `A:\handover-deepseek\restantes-optimizacion-deepseek.md`.
- **Contratos congelados:** tokens aceptados, gramática, requerimiento de llaves en loops, AST,
  slots y children, ranges, condition declarations, dangling-else, bloques embebidos, `defer`,
  recovery/progreso, orden y texto de diagnostics, sintaxis, semántica, API, ABI, NSMETA, CLI y
  artefactos 3.0.4.
- **Archivos y subsistemas protegidos:** lexer, semántica, HIR, MIR, LLVM, runtime, metadata,
  Driver, Builder, fixtures externos, manifiestos y expectativas ajenas a la profundidad R04B.
- **Prohibido:** ampliar a `foreach`, `switch`, `match`, loop/comprehension, `try`, `using` o
  `defer`; cambiar una expectativa para acomodar otro AST; aumentar stack o introducir un guard;
  baseline, suite completa, deploy, instalación, push, pull o fetch.
- **Paralelismo:** no aplica; el cursor y la materialización AST conservan orden fuente.
- **Gate focal:** reproducir profundidad cruzada antes del cambio; conservar las 87 comprobaciones
  anteriores y sumar regresiones profundas de `do` y mezcla R04B; construir `nsc`; ejecutar los
  componentes parser directamente afectados; comprobar cero autorreentradas por cuerpos R04B;
  auditar diff, documentar y crear un commit local aislado.
- **Terminación:** R04B3 queda cerrado sólo cuando los cuatro controles usan una pila explícita
  compartida y sus AST/diagnostics focales permanecen idénticos. R04 global continúa abierto para
  R04C y R04D; L06 continúa abierto hasta recibir la traza GUI real.

## Presupuesto de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | parser de statements R04B |
| Archivos de producto | `Parser.h`, `Parser.cpp` |
| Símbolos | cuatro entry points, headers/lowerings y continuación compartida |
| Pruebas | regresión profunda focal y componentes parser relacionados |
| Configuración | build existente `build-test-infrastructure` |
| Perfil | O0 focal; no baseline |
| Comportamiento variable | consumo del stack del host |
| Comportamiento inmutable | todos los contratos funcionales enumerados arriba |

## Grafo causal auditado

```text
parseStatement
  -> parseIf/While/DoWhile/For
      -> cuerpo embedded o parseBlock
          -> parseStatement
              -> parseIf/While/DoWhile/For
```

R04B2 corta esa arista solamente cuando el siguiente control es un `if`. R04B3 debe hacer que la
misma cola de trabajo reconozca cualquiera de las cuatro familias tanto en cuerpos embedded como
en bloques, y materialice cada padre cuando su hijo ya esté disponible.

## Estado

### Reproducción previa al cambio

- El target original en `b0141acf0` pasa **87 comprobaciones** en 2.12 s.
- Se agregaron antes del cambio de producto dos regresiones dirigidas: 512 cuerpos `do-while` y
  1,024 cuerpos cruzados `if/while/do/for`.
- El flujo completo con 1,024 cuerpos cruzados termina en **`0xC00000FD`**
  (`-1073741571`). Esa reproducción inicial no contenía instrumentación suficiente para atribuir
  la fase, por lo que no se usa como prueba causal exclusiva del parser.
- La entrada no involucra semántica nueva: utiliza exclusivamente construcciones 3.0.4 que ya
  pasan por separado a menor profundidad.

### Hallazgo de frontera

Después de eliminar la reentrada del parser, el mismo caso de 1,024 niveles completa
`syntax-parse` en 1 ms con el `nsc` construido y se desborda después, en `members`. Por tanto:

- R04B3 no puede adjudicarse ni corregir ese segundo defecto;
- el gate propio del parser usa 4,096 niveles directos, sin pasar por semántica;
- el recorrido integrado conserva 256 niveles para demostrar que el AST producido sigue siendo
  consumible por method-bodies;
- el desbordamiento posterior queda como deuda explícita para la auditoría final, no oculto como
  una supuesta limitación de profundidad aceptable.

## Implementación

1. Los cuatro entry points ordinarios (`if`, `while`, `do`, `for`) delegan a una sola máquina de
   continuaciones respaldada por heap.
2. Los headers y lowerings de `while`, `do-while` y `for` se separaron del recorrido del cuerpo,
   conservando el mismo orden de consumo, slots AST, children, ranges y textos de diagnóstico.
3. La cola reconoce controles estructurados tanto como statement embebido como dentro de un
   bloque; el padre sólo se materializa después de recibir el AST de su cuerpo.
4. La máquina se encapsuló en `Parser::StructuredControlParser` y se dividió en operaciones
   pequeñas. El cambio no deja una función de despacho monolítica de cientos de líneas.
5. Los `if/while (let ...)` permanecen deliberadamente en su ruta existente: no se mezclaron con
   los controles ordinarios sin una prueba específica de su lowering.

## Validación focal

- Construcción `nsc_project_method_body_model_tests`: PASS con `/W4 /WX`.
- Prueba focal directa: **PASS 91/91** en 2.16 s aproximadamente.
  - conserva las 87 comprobaciones anteriores;
  - agrega 512 `do-while` anidados;
  - agrega 4,096 controles `if/while/do/for` mixtos parseados directamente;
  - agrega paridad de forma AST para la mezcla embebida;
  - agrega 256 controles mixtos consumidos por method-bodies.
- Construcción `nsc_parser_tests`: PASS.
- Ejecución `nsc.parser`: 22/23 comprobaciones; la única roja es la expectativa histórica
  `invalid ternary conditional syntax is rejected by NativeSharp 1.0.1`. Su entrada no contiene
  ningún statement R04B y no alcanza las rutas modificadas. No se alteró ni se contó como PASS.
- Construcción `nsc`: PASS; contrato estático del host runtime: PASS.
- Auditoría de llamadas: los wrappers ordinarios sólo delegan a la máquina; los cuerpos R04B
  reconocidos no reentran por `parseBlock`/`parseStatement`.
- Forma AST: `IfStatement -> WhileStatement -> DoWhileStatement -> ForStatement` conserva el
  orden esperado en el volcado focal.
- `git diff --check`: PASS; las líneas nuevas respetan el límite de 120 columnas.
- El target agregado `nsc_component_tests` no existe en la configuración congelada
  `build-test-infrastructure`; no se reconfiguró el árbol para fabricar evidencia.
- No se verificó análisis estático completo porque este build no expone una base de compilación
  para `clang-tidy`; la compilación focal sí trató warnings como errores.

No se ejecutaron baseline, suite completa, deploy, instalación, push, pull ni fetch.

## Cierre y pendientes explícitos

R04B3 y la subfamilia R04B quedan cerrados. R04 global continúa abierto para R04C, R04D y una
auditoría final cruzada R04E. R04E debe comprobar, después de esas migraciones, que no queden
aristas recursivas entre controles ordinarios, `foreach`, condition-pattern, switch/match y
cleanup/exceptions. El desbordamiento observado en `members` también debe permanecer rastreado
fuera de este expediente.
