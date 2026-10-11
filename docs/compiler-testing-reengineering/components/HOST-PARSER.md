# Host.Parser — inventario semántico y contrato reconstructivo

## Estado y alcance

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE` y `HECHO COMPROBADO` por lectura estática.
- **Revisión:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Universo:** dos archivos, 14,462 líneas y 614,860 bytes.
- **Superficie inventariada:** 174 símbolos, 42 contratos y 147 definiciones miembro.
- **Validación ejecutada:** ninguna; sólo auditoría estática y diseño de pruebas.

## 1. Propósito y frontera

`Host.Parser` consume el flujo de `Token` producido por `Host.Lexer`, construye `AstNode` y `TypeSyntax` en un
`AstArena`, registra diagnósticos sintácticos y aplica varios lowering tempranos. Su frontera incluye la
desambiguación sintáctica sensible a nombres de tipo conocidos, la síntesis de declaraciones y cuerpos, y la
recuperación local tras errores. No realiza resolución nominal completa, comprobación de tipos, layout, HIR,
MIR ni codegen, aunque algunos lowering producen formas que esas capas consumen directamente.

## 2. Archivos y ownership

| ID | Archivo | Responsabilidad |
|---|---|---|
| PF-000303 | `include/nativesharp/Parser/Parser.h` | API, tipos auxiliares, gramática privada y estado |
| PF-000818 | `src/Parser/Parser.cpp` | reconocimiento, construcción AST, lowering temprano y recuperación |

El parser no posee tokens, arena ni engine. Los tres deben sobrevivirlo. Los nodos y tipos se asignan en el
arena. `knownTypeNames_` sí es estado propio; incorpora 16 nombres builtin y la semilla externa.

## 3. Cursor, progreso, rangos y diagnóstico básico — `CP-PAR-001..002`

`current` y `peek` saturan en el último token; el contrato presupone un flujo no vacío terminado en EOF.
`consume` no avanza después del último elemento. `parseCompilationUnit` exige progreso: si un parser de
declaración no consume nada, descarta un token. `join` y `tokenRangeFrom` conservan rangos físicos.

`expect` y `expectContextualKeyword` consumen sólo al acertar y, al fallar, emiten `ExpectedToken` sin ocultar
el token actual. `makeErrorExpression`, `synchronizeGlobal` y `synchronizeStatement` proporcionan formas de
recuperación diferentes: expresión error, frontera global (`;`, `}` o inicio de declaración) y frontera de
sentencia (`;` o `}`). El test plan verifica progreso además del diagnóstico.

## 4. Lookahead y desambiguación de tipos — `CP-PAR-003`

`canStartType`, `canStartTypeAt` y `scanTypeAt` reconocen tipos sin construir AST. El scanner cubre qualifiers
de ownership/lifetime, nombres calificados, builtin, `dyn`, `some`, tuplas, tipos función, vectores/máscaras,
argumentos genéricos, punteros y arrays. Mantiene profundidad y puede descomponer `>>`/`>>=` como cierres
genéricos. `looksLikeVariableDeclaration`, `looksLikeConditionDeclaration` y
`looksLikeParenthesizedType` usan ese resultado para separar declaraciones, expresiones y casts.

La semilla `knownTypeNames_` influye sólo en nombres no builtin. `KnownGenericTypeScope` instala temporalmente
parámetros de tipo/pack y retira exactamente los nombres que insertó, sin borrar una sombra exterior.
Un prefijo inequívoco `Identificador Identificador` conserva la ruta de declaración aunque el primer nombre
todavía no figure como tipo conocido; la resolución nominal posterior decide si ese tipo existe.

## 5. Nombres, lifetime y tipos base — `CP-PAR-004..007`

`parseQualifiedName` acepta cadenas de identificadores separadas por `.` y devuelve texto/rango. La anotación
de lifetime opcional exige el spelling contextual y un identificador.

`parseType` construye:

- qualifiers `owned`, `borrowed` y lifetime;
- builtin y nombres calificados;
- `dyn Trait` y resultado opaco `some Trait`;
- tuplas con nombres opcionales;
- tipos función estructurales, carrier, calling convention, varargs, `unsafe`, `noheap`, `async`, nulabilidad,
  lifetime de función y tipo de error;
- SIMD portable y máscaras;
- argumentos genéricos de tipo, constantes y packs;
- punteros con constness, alineación, address space y provenance;
- arrays fijos, dinámicos, por rango, rank y flexible.

`void` objeto/array inválido se rechaza, mientras el puntero a `void` conserva su contrato. La asignación de
tipos para `new`/`stackalloc` se concentra en `parseAllocationElementType`.

## 6. Genéricos y constraints — `CP-PAR-008..011`

La lista de parámetros admite parámetros de tipo, lifetime, constantes, type packs y const packs; detecta
duplicados y exige que un pack sea final. `KnownGenericTypeScope` afecta lookahead sólo dentro de la
declaración correspondiente.

Las cláusulas `where` cubren igualdad de tipos/constantes, proyecciones asociadas, sustitución, outlives y
constraints nominales o contextuales (`class`, `struct`, `new`, copy/move/lifetime/unmanaged/notnull). Se
preservan orden y duplicados diagnósticos.

La lista de argumentos genéricos coopera con `consumeGenericGreater`; `>`, `>>` y `>>=` pueden cerrar uno o más
niveles sin perder el resto léxico. Dos lookaheads separados deciden si `<...>` pertenece a una llamada o a un
receptor estático cerrado, sin convertir comparaciones relacionales en genéricos.

## 7. Imports, namespaces y extensions — `CP-PAR-012..013`

`parseNamespaceMember` enruta imports/reexports, namespace, using, extension y declaración global.
Los imports explícitos se representan con los nodos de using existentes; cubren símbolo, trait, extension,
glob restringido, alias y reexport. Namespace acepta cuerpo con llaves o forma de ámbito por archivo. Using
distingue namespace y alias de tipo completo. Extension registra el receptor y activa un estado de cuerpo
acotado para validar miembros.

## 8. Declaraciones globales y modificadores — `CP-PAR-014`

`parseGlobalDeclaration` consume y valida accesibilidad, `partial`, `abstract`, `sealed`, `static`, `readonly`,
`borrowed`, `copy`, `record`, exportación, `extern C`, inlining, `repr`, `pack`, `align` y `size`. Después enruta a
delegate, class, interface/trait, impl, variant, opaque, struct/union/record, enum, extern, función o global.
Combinaciones sin significado emiten diagnóstico; los atributos válidos se fijan en el AST de salida.

## 9. Delegate y agregados — `CP-PAR-015..018`

- Delegate conserva retorno, nombre, genéricos, parámetros, constraints, throws y terminador.
- Struct/union/record/copy/borrowed admite parámetros primarios, bases/constraints, layout y cuerpo. Los
  parámetros primarios materializan campos y constructor cuando corresponde.
- Variant se baja a struct nominal con tag, unión payload, factories, pruebas y extractores sintetizados. Los
  discriminantes explícitos se diagnostican y la recuperación asigna ordinales sintetizados desde cero.
- Opaque se baja a struct nominal de un campo y operaciones explícitas de frontera; copy sólo se propaga si se
  escribió.

`parseStructBody` comparte el parser de miembros y garantiza cierre/progreso.

## 10. Clases, interfaces, traits y tipos anidados — `CP-PAR-019..020`

Class procesa bases, genéricos, constraints, cuerpo y flags partial/abstract/sealed/static. Un `impl Trait for
Type` produce un `ClassDeclaration` marcado `ns3-trait-impl`: `typeSyntax` conserva el trait y el segundo target
conserva el tipo implementador. Interface y trait admiten miembros, tipos y constantes asociados;
implementación y declaración tienen requisitos distintos.

Los tipos anidados reutilizan los cinco parsers nominales existentes. `NestedTypeModifiers` transporta la
accesibilidad y rechaza modifiers que no tengan significado para un tipo, evitando una gramática paralela.
`new class` anidada es válida y conserva `KwNew`; `static` sólo es válido en class, `readonly` sólo en struct y
`const` se diagnostica como inválido para tipos anidados.

## 11. Miembros y dispatch — `CP-PAR-021..027`

`parseClassMember` es el punto de decisión para:

- atributos de layout/offset y modifiers de miembro;
- bloques `extern C` e imports estáticos;
- tipos anidados, constructores, destructor y eventos;
- operadores, conversiones, propiedades e indexers;
- métodos, campos, bitfields y agregados anónimos.

Eventos conservan tipo, nombre y modifiers. Propiedades/indexers validan accessors, accesibilidad, duplicados,
cuerpo/auto-accessor y retorno por referencia; los automáticos sintetizan campo y métodos get/set. Operadores
validan aridad/modifiers y se nombran con el mapping canónico; conversiones distinguen implicit/explicit.

Constructor admite inicializador `base`/`this`, `noheap`, parámetros, throws y cuerpo; destructor restringe
nombre/firma. Métodos y funciones comparten parámetros, genéricos, constraints, modos FP, throws y unwind,
pero preservan reglas de contexto. `extern C` puede envolver declaraciones o formar callable externo. Variables
globales y funciones se separan después de analizar tipo/nombre.

## 12. Cláusulas de callable y parámetros — `CP-PAR-027`

Modo FP, `throws` y unwind se adjuntan al callable una sola vez y diagnostican formas no admitidas.
Parámetros cubren dirección/ownership, referencia, varargs, default, packs y nombres. El parser conserva el
orden sintáctico; las reglas semánticas posteriores no se confunden con éxito de parseo.

## 13. Bloques, statements y variables — `CP-PAR-028`

`parseBlock` exige llaves, agrega statements y garantiza progreso. `parseStatement` enruta bloque, FP/unsafe,
assembly, patrones condicionales, control, comprehensions, saltos, return, defer, try/throw, using,
`static_assert`, declaración/deconstruction o expresión.

Deconstruction admite nesting, discard, modifiers de binding y anotaciones. Declaraciones locales cubren
`const`, RAII, tipo explícito/inferido, inicializador, listas y aggregate initializer. Un aggregate initializer
preserva spread, designadores y orden.

## 14. Inline assembly — `CP-PAR-029`

Assembly consume líneas raw del lexer, target/dialect/opciones, outputs, inputs y clobbers. Constraints se
validan por rol; placeholders deben referir operandos existentes y el control flow sólo acepta las formas
admitidas. Los diagnósticos específicos distinguen target, constraint, placeholder, clobber y control flow.

## 15. Control de flujo clásico — `CP-PAR-030`

El parser cubre conditional pattern, `if`, `while`, `do/while`, `for` y `switch`. Una declaración condicional
se baja conservando su scope. Switch distingue `default(T)` de etiqueta `default:`, agrupa labels/sections y no
permite fallthrough implícito. `statementAlwaysDiverges` se usa como análisis sintáctico conservador.

## 16. Patrones y match — `CP-PAR-031..032`

La gramática de patrones incluye wildcard, literal, binding por valor/ref/move, tuple, struct, variant, range y
alternativas `or`. Cada patrón conserva rango, campos, valores e hijos. Helpers recursivos detectan bindings,
expanden alternativas, clasifican condiciones, generan predicados y emiten bindings.

Match statement/expression evalúa el scrutinee una sola vez mediante temporal higiénico, baja arms/guards a un
árbol de decisión y preserva payloads. Detecta catchall, solapamientos sintácticos y arm posterior inalcanzable.
El parser marca la cobertura que debe completar Sema; no demuestra exhaustividad nominal por sí solo.

## 17. Loop/comprehension y foreach — `CP-PAR-033..034`

`loop` y comprehension mantienen contexto de builder/label con nombres monotónicos. Yield sólo es válido dentro
de ese contexto. `for` conserva init/condition/increment/body.

Foreach intenta, en orden contractual, lowering de range, array/span y protocolo enumerator. Construye locales,
calls, índices y condiciones sintetizadas con helpers AST; la colección fuente se evalúa una vez. Los nombres
temporales dependen de contadores propios y no deben colisionar entre lowerings.

## 18. Cleanup y exits — `CP-PAR-035`

Return admite valor opcional. `defer` con capturas se baja durante el parseo a try/finally anidado; cada captura
preserva ref/move/value y el orden LIFO. Try admite catches y finally; catch conserva tipo/nombre/filtro.
Throw distingue rethrow y expresión. Using statement materializa el recurso y cuerpo sin duplicar evaluación.

## 19. Precedencia y expresiones — `CP-PAR-036..040`

La cadena de precedencia es assignment, conditional, null-coalescing, range, logical-or/and, bitwise,
equality, relational, shift, additive, multiplicative, unary, postfix y primary. Assignment y `??` son
right-associative; los niveles binarios restantes son iterativos y left-associative. `?` se desambigua entre
ternario y propagación escaneando delimitadores/nested questions.

Lambda tiene lookahead propio, capturas, genéricos, parámetros auto/tipados, retorno y cuerpo expression/block.
Unary incluye aritmética, move, from-end, casts nombrados, type tests, size/align/offsetof/default, new/delete,
destroy y stackalloc. Postfix cubre llamada, argumentos genéricos, index/slice, acceso seguro, `.`, `->`,
post-increment/decrement, propagación y `with`. `->` se normaliza a `(*receiver).member` con marca `arrow`.

Primary cubre literales, identifier, `this`/`base`, tuple/parenthesized, match/loop/comprehension e interpolated
string. Los holes interpolados se vuelven a lexear y parsear como fragmentos; se exige consumo completo y se
trasladan rangos/diagnósticos a la fuente exterior.

## 20. Síntesis, clonación y modo FP — `CP-PAR-041`

Los helpers `make*` crean formas AST canónicas usadas por lowering. `cloneParsedType` y `cloneParsedNode` hacen
copia profunda de tipos/nodos, constraints y children en el arena de destino. `applyFloatingPointModeToTree`
propaga sólo sobre nodos en modo `Inherit`; un override corta esa rama. `signedIntegerLiteral` reconoce enteros
positivos, negativos y `INT64_MIN` sin overflow.

## 21. Estado, recursión y límites — `CP-PAR-042`

Estado mutable: cursor, cierres genéricos pendientes, contadores de match/defer/comprehension, stack de
comprehension, contexto extension y nombres de tipo conocidos. Una instancia no es reentrante ni thread-safe.

La implementación vigente contiene recursión en clonación/propagación AST, divergencia de statements,
deconstruction, patrones, precedencias right-associative, unary y fragmentos interpolados. La profundidad la
controla el input; no existe un límite central explícito en Parser. La Fase 1 no corrige esto: el plan exige
casos profundos y deja la sustitución por worklists para una fase de implementación autorizada.

Los comentarios y diagnósticos conservan nombres históricos como `Language3` y etapas. Se inventarían hechos
si se interpretaran como una barrera de edición: aquí sólo se registra el comportamiento observable actual.

## 22. Dependencias

- Directas: AST, Diagnostics, Lexer, Source, Target, Frontend.IntegerLiteralTyping y varias políticas Language.
- Indirectas: Basic y lifetime del buffer fuente a través de Token.
- Consumers: Sema, Frontend, HIR y cualquier herramienta que consuma el AST.
- La arena y diagnostics pueden fallar/limitar por políticas propias; Parser no los posee.

## 23. Reconstructibilidad

Una reimplementación compatible debe reconstruir primero cursor/diagnóstico/lookahead, luego tipos/genéricos,
declaraciones/miembros, statements/patrones y expresiones; por último los lowerings y recuperación. No basta
aceptar la sintaxis: deben coincidir kind, children, rangos, flags, texto auxiliar, temporales, diagnósticos,
progreso y evaluación única.

`RECONSTRUIBLE` sólo se confirma después de poblar símbolos/contratos/casos, cerrar referencias mecánicas y
realizar la segunda auditoría independiente. No se crearon ni ejecutaron pruebas.
