# NativeSharp 2.0 self-hosting — tanda 06: lexer, sintaxis y parser

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Tareas | `NS2-SH-055` a `NS2-SH-066` |
| Gate | `SH05` |
| Rama | `development` |
| Commit de entrada | `c18ecd61b` |
| Commit de cierre | este commit (`selfhost(tanda-06): port canonical lexer and parser`) |
| Fecha de apertura | 2026-09-19 |
| Fecha de cierre | 2026-09-19 |
| Baseline de cierre | `tanda-06-sh05-final-20260919`, PASS, 530.688 s |

## 2. Objetivo y alcance

### Objetivo

Hacer que el ejecutable NativeSharp creado en SH04 lea NativeSharp 2.0 sin usar el frontend C++ y
publique las dos primeras representaciones canónicas del bootstrap: tokens y árbol sintáctico. El
criterio no es «acepta un ejemplo», sino igualdad observable contra `nsc0`, compilación del mismo
frontend en O0/O2 y terminación acotada ante entradas rotas.

### Incluido

- recorrido byte a byte del buffer original, BOM UTF-8 y offsets semiabiertos;
- trivia, comentarios, keywords, literales, operadores y ensamblador integrado contextual;
- stream de tokens compacto en estructura de arreglos, sin una asignación de heap por token;
- nodos planos propiedad de un árbol, relaciones por ID y congelación después del parseo;
- declaraciones globales y de tipo, funciones, métodos, constructores, propiedades, indexadores,
  operadores, delegados, parámetros, tipos, genéricos y constraints;
- statements, precedencia completa de expresiones, casts, asignaciones y memoria explícita;
- `switch`, excepciones, `using`, `static_assert` y bloques `asm` con operandos y clobbers;
- dumps compatibles con la forma canónica de `nsc0`;
- diferencial de tokens sobre todo el corpus descubierto y de AST sobre dieciséis fuentes
  representativas;
- corpus inválido/truncado con timeout y límite de cascada observable;
- gate CMake/CTest reproducido en O0 y O2.

### No incluido

- binding de nombres, tipos semánticos, overload resolution o ownership; empiezan en SH06;
- evaluación de constantes, layout, ABI o validación semántica de `asm`;
- HIR/MIR/LLVM o producción de ejecutables desde `nsc-next`;
- sustitución del compilador instalado: SH05 produce candidatos aislados;
- atributos con sintaxis de corchetes: esa sintaxis no existe en el parser 2.0 de `nsc0`; los
  atributos observables actuales son modifiers, `aux`, flags, constraints y formas declarativas;
- reescritura de NSLink o NSAsm; siguen siendo herramientas externas y cualquier versión
  NativeSharp vivirá en repositorios separados.

## 3. Diseño

### Arquitectura

El flujo es lineal y sin llamadas ocultas al compilador semilla:

```text
SourceBuffer.RawText
  -> Lexer
  -> TokenStream (kind/begin/end)
  -> Parser
  -> SyntaxTree (IDs planos)
  -> Freeze
  -> TokenDumper / SyntaxDumper
```

`SourceBuffer` conserva dos vistas. `RawText` mantiene bytes, BOM y finales físicos para que los
offsets de tokens sean idénticos al archivo; `Text` conserva la vista LF de SH04 para line maps y
hash estable. El lexer sólo guarda clase y rango: el lexema se obtiene del buffer al volcar. Esto
elimina copias por token y hace imposible que lexema y ubicación diverjan.

`SyntaxTree` es el único propietario de los nodos. Cada nodo almacena IDs enteros para `first`,
`second`, `third` e hijos; no hay punteros propietarios cruzados. El parser puede construir el
grafo y `Freeze()` sella cada nodo. Las mutaciones posteriores se vuelven no-op, por lo que las
fases siguientes reciben una representación inmutable aun bajo O0.

El parser es descenso recursivo. Las expresiones usan niveles explícitos de precedencia; tipos y
llamadas genéricas comparten el manejo de `>`/`>>`; el ensamblador consume tokens físicos emitidos
por el lexer. Los errores incrementan un contador y cada bucle comprueba progreso. El proceso no
pretende hacer semántica: por eso un tipo externo desconocido puede seguir representándose.

### Contratos e invariantes

- todo token tiene `0 <= begin <= end <= RawLength`;
- `EndOfFile` siempre existe y tiene rango vacío;
- trivia nunca aparece en el stream, salvo texto/newline dentro del cuerpo contextual `asm`;
- `asm` sólo cambia de modo cuando un `Identifier "asm"` va seguido del `{` real;
- strings válidos conservan UTF-8; el dumper escapa bytes no ASCII como `\xhh`;
- enteros se validan en radix 2/10/16 contra `ulong`; sufijos admitidos coinciden con `nsc0`;
- un índice de nodo negativo significa ausencia; todos los demás pertenecen al mismo árbol;
- después de `SyntaxTree.Freeze`, no se crean ni modifican nodos;
- un bucle de recuperación consume al menos un token o termina;
- O0 y O2 generan la misma salida canónica;
- CRLF/LF de la consola se normaliza sólo en el comparador, nunca en rangos o lexemas;
- la única exclusión de tokens está declarada con razón en el contrato: la fixture negativa
  `comptime` es prefiltrada por el driver de `nsc0` antes de llegar a su lexer.

### Decisiones y alternativas

| Decisión | Alternativa | Razón |
|---|---|---|
| tres listas paralelas de token | clase por token | evita cientos de asignaciones y ownership individual |
| rangos sobre `RawText` | usar texto LF | CRLF cambiaría offsets respecto del compilador existente |
| IDs planos de nodo | punteros padre/hijo propietarios | ownership único, dump determinista y freeze barato |
| strings de tipo canónicos en SH05 | tabla semántica prematura | resolver tipos pertenece a SH06 |
| descenso recursivo manual | generar Flex/Bison | conserva recovery, contexto `asm`, genéricos y árbol existente |
| comparar stdout real de `nsc0` | expectativas copiadas | el oráculo detecta deriva en ambos lados |
| contrato JSON separado | lista embebida en Python | revisión humana y evolución versionada |
| normalizar sólo newline de proceso | reescribir dumps | hace el gate portable sin ocultar diferencias de contenido |

## 4. Inventario de archivos

| Archivo/familia | Acción | Responsabilidad |
|---|---|---|
| `selfhost/nsc/Lexer/TokenKind.ns` | crear | enum y nombres canónicos de tokens |
| `selfhost/nsc/Lexer/TokenStream.ns` | crear | almacenamiento SoA, rangos y conteo léxico |
| `selfhost/nsc/Lexer/Lexer.ns` | crear | lexer completo, literales, trivia y modo `asm` |
| `selfhost/nsc/Lexer/TokenDumper.ns` | crear | escape y formato `Kind "text" @begin:end` |
| `selfhost/nsc/Syntax/SyntaxTree.ns` | crear | nodos, árbol, freeze y dumper AST |
| `selfhost/nsc/Parser/Parser.ns` | crear | tipos, declaraciones, statements, expresiones y recovery |
| `selfhost/nsc/Source/SourceBuffer.ns` | modificar | conservar además el texto físico |
| `selfhost/nsc/Driver/Program.ns` | modificar | exponer `--dump-tokens` y `--dump-ast` |
| `selfhost/nsc/NativeSharp.Compiler.nsproj` | modificar | incluir Lexer/Syntax/Parser |
| `selfhost/contracts/frontend.contract.json` | crear | corpus, formatos, exclusión y recovery SH05 |
| `selfhost/bootstrap/verify_frontend.py` | crear | diferencial O0/O2 y reporte estructurado |
| `cmake/VerifyNativeSharpFrontend.cmake` | crear | construir, verificar PE y ejecutar gate |
| `CMakeLists.txt` | modificar | registrar `nsc.selfhost.frontend` |
| `tests/selfhost/test_bootstrap_infrastructure.py` | modificar | fijar schema, matriz y newline canónico |
| roadmap y READMEs | modificar | estado, navegación y frontera alcanzada |
| este expediente y evidencia SH05 | crear | diseño, resultados y hashes de cierre |

## 5. Tipos y datos

| Tipo | Responsabilidad | Invariantes | Ownership/vida |
|---|---|---|---|
| `TokenKind` | vocabulario léxico | orden idéntico a `nsc0` | valor |
| `TokenStream` | tokens compactos | listas con igual longitud; EOF final | posee tres listas |
| `Lexer` | estado del recorrido | offset monótono; modo asm balanceado | posee stream hasta `LexAll` |
| `SyntaxNode` | forma sintáctica | IDs -1 o válidos; no muta congelado | propiedad del árbol |
| `SyntaxTree` | arena lógica plana | root válido; freeze transitivo | posee todos los nodos |
| `Parser` | cursor y recovery | cursor monótono o error contado | posee árbol durante parseo |
| contrato frontend | matriz normativa | schema/version 1 | archivo versionado |
| reporte SH05 | evidencia de ejecución | status passed y métricas completas | generado, luego promovido |

## 6. Métodos y funciones

| Firma/grupo | Responsabilidad | Postcondición/errores |
|---|---|---|
| `Lexer.LexAll` | producir stream hasta EOF | transfiere ownership del stream |
| `Lexer.SkipTrivia` | espacios y comentarios | nunca consume token real |
| `Lexer.LexNumber` | radix, float, exponentes y sufijos | `Invalid` ante forma/overflow incorrectos |
| `Lexer.DecodeEscape` | escapes ASCII y `\xHH` | cursor queda después del escape |
| `Lexer.LexAssemblyBody` | texto/newline físico y braces anidados | entrega `}` exterior al parser |
| `TokenFacts.Name` | nombre estable | coincide con enum C++ |
| `TokenDumper.Dump` | serialización token | una línea por token, EOF incluido |
| `SyntaxTree.Create` | alta en arena plana | falla con -1 después de freeze |
| `SyntaxTree.Freeze` | sellado transitivo | ningún setter posterior cambia datos |
| `SyntaxDumper.DumpNode` | preorder canónico | orden first/second/third/children |
| `Parser.ParseType` | ownership, calificación, genéricos, pointer/array | texto de tipo canónico |
| `Parser.ConsumeGenericGreater` | desambiguar `>` y `>>` | soporta genéricos anidados |
| `Parser.ParseAggregate` | class/struct/interface y miembros | nodo declarado y tipo base primario |
| `Parser.ParseFunction` | métodos/funciones, parámetros y where | body o declaración externa |
| `Parser.ParseProperty/ParseIndexer` | accessors y métodos sintetizados | forma idéntica al dumper legado |
| `Parser.ParseOperator` | nombre `op_*` y operación auxiliar | aridad distingue unary/binary |
| `Parser.ParseExpression` | condicional/asignación y precedencia | raíz del subárbol |
| `Parser.ParseSwitch` | secciones y labels agrupados | statements sólo dentro de sección |
| `Parser.ParseTry/ParseCatch` | try, catches y finally | first=try, second=finally, children=catches |
| `Parser.ParseInlineAssembly` | líneas, in/out y clobbers | texto físico + nodos estructurales |
| `Parser.ParseStatement` | despacho y progreso | consume o el bloque fuerza avance |
| `verify_frontend.verify` | corpus diferencial | reporte SH05 o excepción precisa |
| `canonical_lines` | independencia CRLF/LF del proceso | no altera contenido de lexemas |

## 7. Superficie observable

- `nsc-next --dump-tokens <file.ns>`;
- `nsc-next --dump-ast <file.ns>`;
- identidad `nsc-next 2.0.0-bootstrap.sh05`;
- formato de tokens `TokenKind "escaped-lexeme" @begin:end`;
- formato AST preorder con indentación de dos espacios y edges `first/second/third/child`;
- códigos 0 para fuente aceptada y 1 cuando lexer/parser registra errores;
- CTest `nsc.selfhost.frontend`;
- schemas `nativesharp-selfhost-frontend-contract` y `nativesharp-selfhost-sh05-report`, v1.

## 8. Implementación

La implementación empezó congelando la salida real de `nsc0`. El primer lexer NativeSharp igualó
los 224 tokens de `full_frontend.ns`; después se amplió a 114 fuentes de tests y finalmente a todo
el árbol. La única diferencia no léxica apareció en la fixture negativa `comptime`: el driver
semilla elimina construcciones prohibidas antes de `--dump-tokens`. Se registró como exclusión en
vez de deformar el lexer.

El primer AST se implementó para `full_frontend.ns` y alcanzó igualdad de 131 líneas. A partir de
ahí cada extensión se guio por un diferencial mínimo: casts/sizeof para HIR, `switch`, EH,
propiedades/indexadores/operadores, `>>` genérico, constraints y ensamblador. El orden evitó portar
305 KiB de C++ a ciegas y obligó a conservar la forma observable en cada paso.

Dos fallos de desarrollo fueron relevantes. Primero, las sentencias de control NativeSharp exigen
braces aun en cuerpos de una línea; el port se normalizó antes de la primera compilación. Segundo,
un miembro `public const` no avanzaba porque `const` no estaba en el conjunto de modifiers. Se
añadió el modifier y además un guard de progreso por miembro. Ninguno llegó al gate final.

La ejecución CMake independiente produjo candidatos O0/O2 y el gate terminó verde antes de abrir
el baseline. Los artefactos permanecen bajo `selfhost/out`, fuera del commit.

## 9. Pruebas específicas

| Prueba | Tipo | Propósito | Resultado previo al baseline |
|---|---|---|---|
| compilación O0/O2 | positiva | frontend válido en ambos niveles | PASS |
| PE verifier | positiva | ejecutables Windows íntegros | PASS ×2 |
| corpus tokens | diferencial | todas las fuentes `.ns` descubiertas | PASS, 954 comparaciones |
| AST integración | diferencial | CFG/HIR/MIR/SSA/LLVM y frontend | PASS |
| AST lenguaje | diferencial | switch, constraints, properties/operators | PASS |
| AST asm | diferencial | líneas, operands y clobbers | PASS |
| recovery | negativa | truncado, carácter inválido, comment y switch roto | PASS, 10 comparaciones |
| timeout recovery | robustez | nunca colgar | PASS, máximo 5 s/caso |
| cascade bound | robustez | dump no explosivo | PASS, máximo 256 líneas |
| Python infraestructura | contrato | schema, matriz y CRLF/LF | PASS, 18/18 |

## 10. Comparación diferencial

El corpus de tokens contiene 477 fuentes después de excluir directorios generados. Cada una se
compara contra `nsc0` en O0 y O2: 954 comparaciones. Se compara stdout canónico y se ignora el exit
code semántico, porque varias fuentes unitarias necesitan referencias de proyecto que
`--dump-tokens` no carga.

El AST usa dieciséis fuentes y dos optimizaciones: 32 comparaciones. Las doce fuentes de
`tests/integration/valid` cubren el pipeline básico. Las cuatro adicionales fijan `switch`,
genéricos/constraints/delegates, propiedades/indexadores/operadores y `asm`. El stdout de `nsc0`
es autoridad incluso cuando su proceso termina 1 por un tipo externo no cargado, siempre que haya
producido `CompilationUnit`; `nsc-next`, al ser sólo sintáctico en SH05, debe terminar 0.

La normalización permitida es exclusivamente CRLF/CR → LF en la tubería del comparador. Los
offsets, lexemas, escapes, nombres, orden de nodos y flags siguen siendo comparación byte a byte.
La exclusión `comptime.ns` está versionada y explica el prefiltrado del driver antiguo.

## 11. Baseline de cierre

El baseline canónico se ejecutó después de terminar el código, los contratos y la documentación
funcional de SH05:

```text
python -B nscbuilder.py --baseline \
  --config selfhost/bootstrap/bootstrap.config.example.json \
  --run-id tanda-06-sh05-final-20260919 --jobs 16
```

| Dato | Resultado |
|---|---|
| Estado | PASS, 0 fallos |
| Inicio / fin | 2026-09-19 15:01:02 / 15:09:53, UTC-06:00 |
| Duración total | 530.688 s |
| CTest `lld-link` | 442/442 PASS, 198.828 s |
| Fixtures `lld-link` | 148/148 PASS, 54.510 s |
| Triage `lld-link` | 18/18 PASS, 8.606 s |
| CTest NSLink | 442/442 PASS, 166.527 s |
| Fixtures NSLink | 148/148 PASS, 52.992 s |
| Triage NSLink | 18/18 PASS, 8.490 s |
| Gate 0 | LANDED, 8.294 s |
| Fuente auditada | 1,394 archivos; SHA-256 `4011079e6b3319a76d035f65e9804d13c3740abe2b3843f10769d2b743a317ef` |
| `nsc0` construido/instalado | SHA-256 `d2eeb7abcadb802a1bf2802a0fef148d62f666bfa6e53ee70ec02b5c288f6561` |
| Reporte Markdown | SHA-256 `e96bebc569021d4b66c8b3670135be2174f430ae268634e67932558f7cbe7cf1` |
| Reporte JSON | SHA-256 `a3f71f73af8e5545f9baf61ea7598fd9af10cfce9556d7f2fd1c0fda2a28cf2a` |
| Gate G00 | SHA-256 `82baebed04b5901e6c47f2211165b0e2c5e06f4f72b1a7cb5510406b19c55774` |
| Reporte SH05 promovido | SHA-256 `739744a66577e7200c228d2e45863f28c942039e0e08f73aa2f1c794dfae37a9` |

El CTest `nsc.selfhost.frontend` pasó dentro de las dos matrices. En cada ejecución verificó 477
fuentes, 954 comparaciones de tokens, 32 comparaciones AST y 10 comparaciones de recuperación. Los
dos candidatos PE quedaron en `selfhost/out` y no se versionan; sus hashes se fijan en el gate
SH05. El reporte completo permanece en
`selfhost/evidence/runs/tanda-06-sh05-final-20260919/` como evidencia local ignorada.

## 12. Riesgos, limitaciones y deuda

- El árbol plano prioriza cierre bootstrap; SH06 podrá tipar campos hoy representados como texto.
- El parser recupera y limita cascadas, pero SH04 sólo tiene dos diagnósticos renderizados; los
  diagnósticos sintácticos con código estable se amplían junto con SH06 sin cambiar el gate SH05.
- El corpus de AST es representativo y canónico, no una afirmación semántica sobre las 477 fuentes.
- La validación profunda de constraints, clobbers, labels asm y tipos base pertenece a semántica.
- Los accessors automáticos conservan forma sintáctica; su backing field y lowering son posteriores.
- `nsc0` sigue siendo semilla/oráculo temporal; no se distribuye dentro de `nsc-next`.

## 13. Contenido del commit

### Higiene

- [x] Los binarios y fixtures generados viven bajo `selfhost/out` ignorado.
- [x] No se incluye ningún `.obj`, `.exe`, `.dll`, `.lib`, PDB o caché.
- [x] Los cambios locales ajenos enumerados al abrir la tanda no se prepararán.
- [x] El manifiesto/fingerprint de fuentes se regeneró por el baseline final.
- [x] Archivos, tipos, métodos y decisiones están documentados.
- [x] El baseline se ejecutó después del último cambio funcional y documental de diseño.

### Commit de cierre

```text
selfhost(tanda-06): port canonical lexer and parser
```

## 14. Criterio de cierre

- [x] `NS2-SH-055` a `NS2-SH-066` tienen implementación y contrato.
- [x] Gate específico O0/O2 verde.
- [x] Diferencial de tokens, árboles y recovery verde.
- [x] Baseline completo verde en lld-link y NSLink.
- [x] Gate SH05 promovido con hashes finales y problemas vacíos.
- [x] Expediente y commit coinciden.
