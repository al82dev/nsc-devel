# 03 — Índice maestro hasta NativeSharp 1.0 — Revisión 2

## Cómo leer este índice

Éste es el plan de contenido, no el contenido terminado. Cada entrada es un capítulo futuro y
define su pregunta central, el incremento que reconstruirá y la evidencia mínima que deberá
entregar. Durante la redacción, un capítulo podrá dividirse si el delta deja de ser revisable en
una sola sesión; nunca se fusionarán etapas sólo para reducir el número de archivos.

Los nombres propuestos usan tres dígitos para que el orden sobreviva a nuevas inserciones. Las
carpetas por volumen contendrán código de checkpoint, fixtures y resultados además del Markdown.

Este índice conserva la numeración 000–210 de la primera planificación. Las inserciones nacidas de
la inspección del archivo histórico se enumeran como 019A, 093B, etc. en
[`10_AMPLIACIONES_DEL_INDICE_MAESTRO_REV2.md`](10_AMPLIACIONES_DEL_INDICE_MAESTRO_REV2.md). La fuente
asignada a cada rango se consulta en
[`11_MATRIZ_FUENTES_HISTORICAS_A_CAPITULOS.md`](11_MATRIZ_FUENTES_HISTORICAS_A_CAPITULOS.md).

---

# Volumen I — Nacimiento y contrato del lenguaje

## Capítulo 000 — Antes del código: el problema que NativeSharp quería resolver

- Recuperar la motivación original desde las primeras conversaciones.
- Separar deseos iniciales de metas añadidas después.
- Explicar “sintaxis familiar, ejecución nativa, costos visibles, sin CLR” sin convertirlo en
  marketing.
- Entregable: manifiesto de diseño inicial con citas y nivel de confianza.

## Capítulo 001 — Qué se toma de C# y qué se rechaza

- Sintaxis, modelo nominal, métodos, properties, delegates y excepciones como inspiración.
- Ausencia deliberada de IL, JIT obligatorio, GC obligatorio y reflection dinámica.
- Diferencias que afectan código fuente, ABI y lifetime.
- Entregable: tabla de compatibilidad dirigida, no una promesa de clon completo.

## Capítulo 002 — Windows x64 como frontera inicial

- Por qué un solo target permite congelar tamaños, ABI, COFF, PE y SEH.
- Qué compromisos se adquieren con `x86_64-pc-windows-msvc`.
- Qué no se abstrae prematuramente.
- Entregable: `TargetContract.md` reconstruido.

## Capítulo 003 — Diseñar el pipeline antes de implementarlo

- Frontend propio, HIR, MIR, CFG/SSA, LLVM, COFF, linker y runtime.
- Por qué LLVM es backend y no semántica del lenguaje.
- Dónde deben vivir type checking, layout, ownership y overload resolution.
- Entregable: diagrama de responsabilidades y lista de invariantes entre capas.

## Capítulo 004 — Especificación NativeSharp procedural 0.1

- Programa, funciones, globals, locals, statements y expresiones.
- Tipos primitivos, structs, enums, arrays y punteros.
- Control manual de memoria e interoperabilidad C.
- Frontera explícita: todavía no hay OOP.
- Entregable: corpus de programas válidos e inválidos que define la superficie inicial.

## Capítulo 005 — Alfabeto, UTF-8, trivia e identificadores

- Bytes frente a caracteres; normalización no asumida.
- whitespace, LF/CRLF/CR, comentarios y final de archivo.
- Identificadores y palabras reservadas.
- Entregable: tabla byte a byte de ejemplos de entrada.

## Capítulo 006 — Literales sin magia

- Enteros, sufijos, bases, floating, char, string y escapes.
- Por qué `-42` son dos tokens.
- Errores de overflow léxico frente a semántico.
- Entregable: matriz de lexemas, tokens y diagnostics.

## Capítulo 007 — Operadores, punctuation y maximal munch

- Inventario completo de `TokenKind`.
- Desambiguación de operadores de uno, dos y tres caracteres.
- `delete[]` como tokens separados.
- Entregable: autómata/tabla de decisión del scanner.

## Capítulo 008 — Precedencia y asociatividad

- Jerarquía desde assignment hasta primary.
- Prefix/postfix, casts, indexing, member access y calls.
- Casos que parecen C# pero todavía quedan fuera.
- Entregable: árboles esperados para expresiones ambiguas visualmente.

## Capítulo 009 — EBNF de compilation units y declaraciones

- Globals, functions, structs, enums y `extern C`.
- Tipos compuestos y modificadores de izquierda a derecha.
- Regla `KnownTypeNames` y declaración antes de uso.
- Entregable: derivaciones completas desde tokens a producciones.

## Capítulo 010 — EBNF de statements y expresiones

- Blocks obligatorios, `if`, `while`, `for`, `break`, `continue`, `return`.
- Declarations frente a expression statements.
- Calls, casts, `sizeof`, `new`, `delete` y `stackalloc`.
- Entregable: grammar tests positivos/negativos en papel antes del parser.

## Capítulo 011 — Identidad y universo de tipos

- Builtins, punteros, arrays, function types, structs, enums y literales contextuales.
- `bool`, `char`, `nint/nuint` y diferencias semánticas aunque compartan tamaño.
- Interning y canonicalización como requisito futuro.
- Entregable: tabla de identidad/equivalencia.

## Capítulo 012 — Conversiones y operadores

- Promotions, conversions estándar, constant conversions, pointer conversions y `null`.
- Signed/unsigned, comparaciones y arithmetic common type.
- Evaluación left-to-right.
- Entregable: matriz origen/destino con resultado y diagnóstico.

## Capítulo 013 — Overflow, división, shifts y traps

- Wrap definido, división por cero, `MIN / -1`, shifts fuera de rango.
- Diferencia entre comportamiento definido, trap y diagnóstico compile-time.
- Entregable: programas que distinguen O0 y O2 y exigen el mismo resultado.

## Capítulo 014 — Lvalues, rvalues, const, arrays y pointers

- Addressability, assignment targets, array non-decay y pointer arithmetic.
- One-past-the-end y bounds.
- Entregable: tabla de categoría de valor por expresión.

## Capítulo 015 — Layout físico de los tipos

- `size`, `align`, padding, stride, field offsets y tail padding.
- Globals, stack, heap y secciones conceptuales.
- Entregable: calculadora manual y golden layouts.

## Capítulo 016 — Lifetime y memoria procedural

- Locals, `stackalloc`, `new/new[]`, `delete/delete[]`, null y allocation failure.
- Dangling pointers, use-after-free, double delete y límites de diagnóstico.
- Entregable: estados de storage y casos de error.

## Capítulo 017 — ABI C y Microsoft x64

- Registers, shadow space, stack alignment, retorno y name linkage.
- Scalars, pointers, aggregates directos/indirectos y sret.
- Entregable: llamadas dibujadas registro por registro y fixtures C equivalentes.

## Capítulo 018 — Contrato de COFF, PE y runtime inicial

- Qué debe producir el compilador y qué hace el linker.
- Imports CRT/Win32, entry point y producto PE32+.
- Entregable: mapa mínimo del binario que las etapas posteriores completarán.

---

# Volumen II — De una carpeta vacía al primer frontend

## Capítulo 019 — Reconstruir el directorio y el `main.cpp` vacío

- Árbol mínimo, licencia/README si existían, CMake inicial y C++20.
- Primer `main` que retorna cero; por qué no introduce todavía el compilador entero.
- Entregable: checkpoint `C000`, build desde cero y hash del ejecutable.
- Nota Rev2: este corte es pedagógico; el primer árbol C++ observado ya es Stage 06 y deberá usarse
  como prueba posterior de convergencia.

## Capítulo 020 — El primer test y la primera regla de warnings

- `enable_testing`, test smoke y warnings-as-errors por toolchain.
- Diferencia entre compilar y verificar comportamiento.
- Entregable: test rojo deliberado, corrección y test verde.

## Capítulo 021 — De `main` a driver

- Separar parsing de argumentos, salida y código de retorno.
- `--help`, `--version`, archivo ausente y opción desconocida.
- Entregable: primera clase/función de driver y pruebas de CLI.

## Capítulo 022 — Leer bytes sin entender lenguaje

- Apertura de archivos, errores I/O, conservación exacta de bytes.
- Por qué todavía no se calculan tokens.
- Entregable: programa que carga y reporta tamaño.

## Capítulo 023 — `FileId`, buffer y ownership de fuentes

- Evitar punteros inestables y paths repetidos.
- Contrato de vida de buffers.
- Entregable: primer `SourceManager` mínimo.

## Capítulo 024 — `SourceLocation` como offset

- Por qué no guardar línea/columna en cada ubicación.
- Ubicaciones inválidas y comparación dentro de un archivo.
- Entregable: tests de offsets y rangos.

## Capítulo 025 — Índice de líneas y finales mixtos

- LF, CRLF, CR, última línea sin newline y archivos vacíos.
- Búsqueda binaria de línea/columna.
- Entregable: tabla exhaustiva de offsets.

## Capítulo 026 — `SourceRange` y fragmentos

- Rangos half-open, tokens vacíos y EOF.
- Extracción segura de snippets.
- Entregable: tests de fronteras.

## Capítulo 027 — Primer diagnóstico estructurado

- ID, severity, mensaje, ubicación y notas.
- Engine separado de renderer.
- Entregable: error sin color y código de salida no cero.

## Capítulo 028 — Renderer de diagnósticos

- `path:line:column`, línea fuente, caret, ranges y tabulación.
- Diagnostics deterministas y testeables.
- Entregable: golden outputs.

## Capítulo 029 — `TokenKind`, `Token` y EOF

- Primer modelo léxico; lexeme como vista del source.
- EOF siempre emitido y progreso garantizado.
- Entregable: archivo vacío produce exactamente EOF.

## Capítulo 030 — El cursor del lexer

- `peek`, `advance`, lookahead, posición inicial/final.
- Invariante: todo camino consume o termina.
- Entregable: scanner de un carácter con tests.

## Capítulo 031 — Trivia y comentarios

- Whitespace, comentarios de línea/bloque y comentario sin cerrar.
- Decisión de conservar o descartar trivia.
- Entregable: tokens idénticos con trivia distinta.

## Capítulo 032 — Identificadores y keywords

- Escaneo primero como identifier, reclasificación por tabla.
- Keywords exactas y prefijos que no lo son.
- Entregable: tests generados desde la tabla canónica.

## Capítulo 033 — Números

- Separar reconocimiento, texto y posterior conversión semántica.
- Bases, punto decimal, exponente y sufijos.
- Entregable: corpus de bordes y diagnostics.

## Capítulo 034 — Char y string

- Escapes, newline ilegal, terminación y UTF-8 válido.
- Entregable: valores decodificados y rangos originales.

## Capítulo 035 — Operadores y maximal munch implementado

- Tabla/`switch`, lookahead y orden de prioridad.
- Unknown character con recuperación.
- Entregable: lexer completo y dump estable.

## Capítulo 036 — Arena y jerarquía mínima de AST

- Por qué ownership por arena simplifica árboles.
- Node kind, source range y visitantes/dumps.
- Entregable: arena sin parser y tests de lifetime.

## Capítulo 037 — Parser de una función mínima

- Recursive descent, token cursor, `expect` y sincronización.
- `int Main() { return 0; }` como primer árbol.
- Entregable: dump exacto del AST.

## Capítulo 038 — Tipos sintácticos

- Builtins, nombres, pointers, arrays y orden de modificadores.
- Syntax no equivale todavía a type semántico.
- Entregable: `TypeSyntax` y dumps.

## Capítulo 039 — Declaraciones completas

- Globals, functions, parameters, structs, enums y extern blocks.
- Entregable: compilation unit con múltiples declarations.

## Capítulo 040 — Statements y scopes sintácticos

- Blocks, local declarations, conditions, loops y jumps.
- Recuperación en `;` y `}`.
- Entregable: corpus de errores múltiples en una sola corrida.

## Capítulo 041 — Pratt/recursive precedence para expresiones

- Implementar cada nivel sin un mega-`parseExpression` opaco.
- Unary, postfix, calls, indexing, members y assignments.
- Entregable: árboles que prueban precedencia/asociatividad.

## Capítulo 042 — Tabla de símbolos y scopes semánticos

- Declarar antes de resolver; duplicados; namespaces todavía no.
- Entregable: lookup lexical procedural.

## Capítulo 043 — `TypeContext` e interning

- Tipos canónicos, qualifiers y estabilidad de punteros internos.
- Entregable: igualdad por identidad donde corresponda.

## Capítulo 044 — SemanticModel separado del AST

- Asociar tipos, symbols, conversions y value categories sin mutar sintaxis.
- Entregable: consultas por nodo.

## Capítulo 045 — Expresiones y conversiones semánticas

- Operators, calls, casts, literals y constante.
- Entregable: diagnostics específicos y suppression de cascadas.

## Capítulo 046 — Statements semánticos y control

- Conditions booleanas, return compatibility, break/continue context.
- Entregable: frontend procedural Stage 10 verificable.

## Capítulo 047 — Auditoría del frontend 06–10

- Reconstrucción desde cero con dos compiladores y sanitizers.
- Libro histórico 00–10 como fuente, no como sustituto de tests.
- Entregable: snapshot certificado del primer volumen implementable.

---

# Volumen III — Representaciones intermedias y compilación procedural completa

## Capítulo 048 — Por qué no bajar el AST directamente a LLVM

- Separación de intención, operaciones físicas y backend.
- Qué información se pierde si se salta HIR/MIR.
- Entregable: contratos AST→HIR y HIR→MIR.

## Capítulo 049 — El primer nodo HIR

- IDs, types, values y operations resueltas.
- Entregable: `return 0` en HIR.

## Capítulo 050 — HIR de variables, calls y memoria

- Lvalue/rvalue explícitos, conversions, bounds y allocation intent.
- Entregable: dump determinista.

## Capítulo 051 — `HirVerifier`

- Verificar producer/consumer, types y terminators independientemente del builder.
- Entregable: mutaciones negativas que el verifier rechaza.

## Capítulo 052 — MIR no-SSA

- Temporales, instrucciones, blocks y operaciones físicas.
- Entregable: HIR→MIR para funciones procedurales.

## Capítulo 053 — `MirVerifier` y dumps

- Invariantes de tipos, operandos y terminación.
- Entregable: corpus válido e inválido.

## Capítulo 054 — Construir el CFG

- Successors, predecessors, entry, exit y bloques inalcanzables.
- Entregable: CFG para `if`, loops y returns múltiples.

## Capítulo 055 — Recorridos y verificación del CFG

- DFS, reverse postorder y simetría pred/succ.
- Entregable: visualización textual y tests independientes.

## Capítulo 056 — Dominadores desde primeros principios

- Definición, algoritmo iterativo y dominador inmediato.
- Entregable: árboles para diamante, loop y bloque inalcanzable.

## Capítulo 057 — Dominance frontier

- Por qué aparece y cómo decide ubicación de phi.
- Entregable: cálculo manual frente a implementación.

## Capítulo 058 — Promotabilidad y construcción SSA

- Qué locals pueden promoverse; definición única y renaming stacks.
- Entregable: primer phi correcto.

## Capítulo 059 — `SsaVerifier`

- Dominance de defs, aridad de phi, edges y type consistency.
- Entregable: pruebas negativas fabricadas.

## Capítulo 060 — AnalysisManager y PassManager

- Preservación/invalidation de análisis.
- Pipeline configurable y determinista.
- Entregable: pass vacío observable.

## Capítulo 061 — Primeras optimizaciones propias

- Constant propagation, folding, copy propagation, branch simplification y DCE.
- Entregable: before/after con verifier entre passes.

## Capítulo 062 — Cargar la API C de LLVM de forma controlada

- Frontera manual, versiones y errores.
- Entregable: módulo LLVM vacío verificable.

## Capítulo 063 — Lowering de tipos y funciones

- Target data layout, signatures, blocks y values.
- Entregable: `Main` que retorna constante en LLVM IR.

## Capítulo 064 — Lowering de control y memoria

- Branches, phi, loads/stores, calls, pointer arithmetic y traps.
- Entregable: programa procedural no trivial verificado por LLVM.

## Capítulo 065 — TargetMachine x86-64 y COFF

- Triple, CPU/features, data layout, object emission.
- Entregable: primer `.obj` AMD64 inspeccionado.

## Capítulo 066 — Runtime mínimo de Windows

- Start, memory, console/traps e imports.
- Entregable: módulo runtime y contrato de símbolos.

## Capítulo 067 — Del `.obj` al PE32+

- LLD invocation/in-process boundary, imports y entry point.
- Entregable: primer `.exe` estructuralmente válido.

## Capítulo 068 — Verificadores COFF y PE

- Machine, sections, symbols, imports y razón de no confiar sólo en exit code.
- Entregable: corrupción deliberada detectada.

## Capítulo 069 — Optimización LLVM O0–O3

- New Pass Manager, target-aware pipeline y equivalencia observable.
- Entregable: IR y objetos por nivel.

## Capítulo 070 — Capstone procedural Stage 20

- Recorrido completo `.ns → .exe`.
- Builds GCC/Clang, sanitizers, manifest, ZIP, extracción y rebuild.
- Entregable: baseline procedural reproducible.

---

# Volumen IV — Del archivo aislado al proyecto y a los objetos

## Capítulo 071 — Especificar OOP antes de implementarlo

- Object identity, classes, accessibility, inheritance prevista y exclusiones.
- Entregable: contrato Stage 21 y preguntas aplazadas.

## Capítulo 072 — Formato `.nsproj`

- Name, Compile, ProjectReference, paths y diagnósticos.
- Entregable: loader mínimo determinista.

## Capítulo 073 — Descubrimiento de fuentes

- Orden estable, duplicados, canonical paths y source sets.
- Entregable: proyecto multifichero sin semántica cross-file.

## Capítulo 074 — Project graph

- Dependencias transitivas, diamonds, ciclos y módulos duplicados.
- Entregable: topological dependencies-first.

## Capítulo 075 — Una compilation unit por archivo

- SourceManager global, lexer/parser por archivo y arenas.
- Entregable: `ProjectCompilation` dump.

## Capítulo 076 — Namespaces y predeclaración global

- Block/file namespace, namespace tree y type seeds.
- Entregable: todos los nombres conocidos antes del binding.

## Capítulo 077 — `using`, aliases y lookup

- Qualified ladder, ambigüedad y visibilidad public/internal.
- Entregable: matriz de resolución cross-project.

## Capítulo 078 — `class` y `partial class`

- Syntax, nominal identity y agregación cross-file.
- Entregable: duplicados, partial mismatch y type identity.

## Capítulo 079 — Member declaration space

- Fields, methods, overload groups, accessibility y `this`.
- Entregable: modelo independiente de cuerpos.

## Capítulo 080 — Scopes de métodos y shadowing

- Parameters, locals, blocks, `for` scope y lookup implícito de members.
- Entregable: eventos de shadowing explícitos.

## Capítulo 081 — Conversiones y overload resolution

- Viability, ranking argumento por argumento y ambigüedad.
- Entregable: no sumar ranks; suppression de cascadas.

## Capítulo 082 — Constructores

- Declaration space, cuerpo, `this`, accesibilidad y default implícito.
- Entregable: `new C(args)` resuelto cross-file.

## Capítulo 083 — Layout de objetos

- Referencia de clase, header, offsets, padding, inline structs/arrays y allocation size.
- Entregable: golden layouts Stage 29.

## Capítulo 084 — OOP HIR

- Receiver, fields, calls, construction y delete.
- Entregable: modelo separado de HIR procedural.

## Capítulo 085 — OOP MIR

- Operaciones físicas, `this` ABI implícito y addresses.
- Entregable: verifier y dump.

## Capítulo 086 — OOP LLVM y COFF

- Allocation, constructor call, field address, instance call y product emission.
- Entregable: capstone Stage 30 sin herencia.

---

# Volumen V — Herencia, dispatch y superficie OOP rica

## Capítulo 087 — Receivers generalizados

- Local, parameter, temporary y call result como receiver.
- Null checks y trap reason 7.
- Entregable: receiver conservado AST→LLVM.

## Capítulo 088 — Grafo de herencia simple

- Base resolution, ciclos, accessibility, inherited lookup y upcasts.
- Entregable: `ProjectInheritanceModel`.

## Capítulo 089 — Base subobject y constructor chaining

- Base en offset cero, un header, `base(...)`, `this(...)` y orden.
- Entregable: layouts derivados verificables.

## Capítulo 090 — Slots virtuales y vtable

- Stable slot assignment, override checks, direct base call e indirect dispatch.
- Entregable: descriptor y llamada virtual LLVM.

## Capítulo 091 — Abstract, sealed y destrucción dinámica

- Abstract slots, concreción, sealed override, destructor chain y free exactly once.
- Entregable: `ProjectDestructionModel`.

## Capítulo 092 — Interfaces y dispatch

- Interface identity, implementation map, conversion y call.
- Entregable: interface map en descriptor.

## Capítulo 093 — El incidente Stage 35/36 Recovery

- Narración declarada frente a snapshot físico.
- Auditoría, recuperación, tests retroactivos y cambio de Definition of Done.
- Entregable: cronología forense completa.

## Capítulo 094 — Statics, cctor y estado de inicialización

- Static fields/methods, initialization ordering, state 0/1/2 y reentrancia.
- Entregable: `__nsc_cctor_<Type>` y tests multithread posteriores.

## Capítulo 095 — `const`, `readonly` y evaluator

- Width, sign, wrap, division, shift y el bug de `true` no inicializado.
- Entregable: evaluator con golden constants.

## Capítulo 096 — Properties

- Accessors explícitos, auto-properties, backing fields y accessibility.
- Entregable: property → `get_X`/`set_X` a través del pipeline.

## Capítulo 097 — Indexers

- Multi-parameter, overloads, get/set y `this[...]`.
- Entregable: lowering como callables.

## Capítulo 098 — Operators y conversiones de usuario

- Static callables, candidate sets y conversion chains controladas.
- Entregable: operador fuente hasta LLVM.

## Capítulo 099 — RTTI nativo

- Stable type IDs, base descriptor, `is`, `as`, downcast y checked cast.
- Entregable: type test sin reflection dinámica.

---

# Volumen VI — Valores, callables AOT y excepciones nativas

## Capítulo 100 — `struct` como valor físico

- Inline bytes, copy semantics, receiver por dirección y recursión ilegal.
- Entregable: class/reference frente a struct/value.

## Capítulo 101 — ABI Win64 de agregados

- Carriers 1/2/4/8, indirect arguments, sret y comparación con Clang/MSVC ABI.
- Entregable: matriz de layouts y llamadas.

## Capítulo 102 — Arrays dinámicos

- Header/length/data, indexing, bounds, allocation y ownership posterior.
- Entregable: array source→PE.

## Capítulo 103 — String nativo y Core mínimo

- Representación UTF-8 observada, literals, operations y frontera runtime.
- Entregable: primer programa con salida textual.

## Capítulo 104 — Delegates

- Cell `(code, receiver)`, creación, invocation y lifetime inicial.
- Entregable: direct/static/instance targets.

## Capítulo 105 — Events

- Linked nodes, add/remove/invoke y comparación de cells.
- Entregable: orden y eliminación de handlers.

## Capítulo 106 — Lambdas sin captura

- Función sintética, receiver null y callable identity.
- Entregable: lambda simple end-to-end.

## Capítulo 107 — Closures con captura

- Environment, slots, snapshot inicial y límites.
- Entregable: capture load y lifetime explícito.

## Capítulo 108 — Genéricos AOT: identidad y cache

- Templates, specialization keys, recursion y cache-before-processing.
- Entregable: `Box<int>` y `Box<long>` concretos.

## Capítulo 109 — Monomorfización de tipos y métodos

- Synthetic AST, name lookup, Sema, HIR/MIR ordinarios y LLVM sin `T`.
- Entregable: generic method y nested specialization.

## Capítulo 110 — Sintaxis y semántica de excepciones

- `try`, catches tipados, catch-all, `finally`, `throw`, rethrow y `using`.
- Entregable: scopes y cleanup obligations.

## Capítulo 111 — EH Win64 en LLVM

- `__C_specific_handler`, invoke, catchswitch/catchpad, cleanuppad/cleanupret.
- Entregable: LLVM IR explicado instruction por instruction.

## Capítulo 112 — RTTI, TLS y matching de excepciones

- Current exception, type matching, normal/unwind cleanup y imports.
- Entregable: ejecución y estructura `.pdata/.xdata`.

## Capítulo 113 — Fallos encontrados durante Stage 45

- Imports, layouts de handlers, catches múltiples, finally omitido y regresiones O0/O2.
- Entregable: catálogo de bugs y tests que los conservan.

---

# Volumen VII — Bibliotecas, interoperabilidad y ownership

## Capítulo 114 — De EXE a productos de biblioteca

- StaticLibrary, SharedLibrary, `.lib`, DLL, import library y runtime `/noentry`.
- Entregable: Stage 46 con verificadores de archive/PE.

## Capítulo 115 — Stable C ABI

- `export extern C`, public name, internal mangling, thunks y publishable types.
- Entregable: host C real.

## Capítulo 116 — Header C desde verdad semántica

- Type mapping, opaque handles, callbacks y ownership visible.
- Entregable: header reproducible, nunca inferido del PE.

## Capítulo 117 — Nacimiento de NSMETA

- Por qué exports no reconstruyen overloads, namespaces, layouts ni ownership.
- Writer, reader, verifier, checksum y fingerprint.
- Entregable: formato temprano y round-trip.

## Capítulo 118 — Consumidores C, C++, C# y C++/CLI

- Qué prueba cada host y qué depende del entorno.
- Entregable: matriz Stage 48.3–48.6.

## Capítulo 119 — RAII léxico

- Owner local, reverse destruction, normal/exceptional cleanup y polymorphic destroy.
- Entregable: primer `raii Resource`.

## Capítulo 120 — `move` y state machine

- Alive/Moved/Destroyed, null source, destroy-before-overwrite, joins y loops.
- Entregable: exactamente un destructor.

## Capítulo 121 — `owned` y `borrowed` en el type system

- Contrato semántico distinto, representación física compartida.
- Entregable: `OwnershipKind` y canonicalización.

## Capítulo 122 — Parámetros y retornos owned

- Transferencia, forwarding, `move`, creación y diagnostics de copia.
- Entregable: funciones productoras/consumidoras.

## Capítulo 123 — Borrows y lifetimes léxicos

- Provenance, root owner, valid scope y borrow-to-borrow.
- Entregable: grafo de borrow bindings.

## Capítulo 124 — Ownership a través del control de flujo

- Alive/Moved/MaybeMoved; Valid/Invalidated/MaybeInvalid; joins y back-edges.
- Entregable: análisis path-sensitive conservador.

## Capítulo 125 — Escape analysis

- Return, fields, statics, closures, callbacks y llamadas que pueden retener.
- Entregable: rejected escapes con diagnostics concretos.

## Capítulo 126 — Ownership en HIR, MIR y LLVM

- BorrowValue, MoveValue, cleanup de owned parameters y borrow como identidad física.
- Entregable: una traza end-to-end sin double free.

## Capítulo 127 — Ownership en metadata y hosts

- NSMETA 1.1+, diferencia `T/owned T/borrowed T/T*` y ABI C estable.
- Entregable: round-trip cross-module.

## Capítulo 128 — Certificación completa de Stage 50

- Todas las subetapas, regresiones Stage 45/49, fresh extraction y package hygiene.
- Entregable: snapshot ownership certificado.

---

# Volumen VIII — Core, colecciones y servicios del sistema

## Capítulo 129 — Construir `NativeSharp.Core.String`

- Superficie, algoritmos, allocation, ownership y runtime hooks.
- Entregable: documentación normativa y de implementación separadas.

## Capítulo 130 — Colecciones genéricas

- List, Queue, Stack, Dictionary/PriorityQueue según el snapshot; resize y ownership.
- Entregable: cada estructura desde invariantes y complejidad.

## Capítulo 131 — `default(T)`, `var` y correcciones del compilador

- Por qué una biblioteca fuerza nueva semántica del lenguaje.
- Entregable: del fallo consumidor al cambio frontend/backend.

## Capítulo 132 — `foreach` como reescritura

- Array path, enumerator pattern, temporales higiénicos y ausencia de nodo MIR específico.
- Entregable: AST candidato → Sema → HIR normal.

## Capítulo 133 — Revisión técnica de `foreach`

- Source owned, readonly iteration variable, `continue`, enumerator lifetime y bug owned move.
- Entregable: primera implementación, crítica, corrección y regresión.

## Capítulo 134 — Tiempo y entorno

- NativeClock, tick count, processor count, exit y environment variables.
- Entregable: Core→runtime→Win32 por operación.

## Capítulo 135 — Archivos, paths y argumentos

- Exists/delete/create, text/binary content, full path, args y directory enumeration.
- Entregable: matriz de éxito/fallo y limitaciones ANSI/UTF-8 observadas.

## Capítulo 136 — Threads y trampoline

- Create, join, detach, entry delegate y ownership del handle.
- Entregable: ejecución repetida y cleanup.

## Capítulo 137 — Atómicos

- LLVM atomic instructions, ordering y por qué no basta envolver Win32.
- Entregable: operaciones 32/64 y tests concurrentes.

## Capítulo 138 — Mutex y cctor reentrante

- Lock/unlock, handle lifetime y el self-deadlock del primer guard atómico.
- Entregable: owner thread state y regression.

## Capítulo 139 — Monitor, semaphore y event

- SRW/condition variable y handles waitables.
- Entregable: timeout/wake semantics dentro del alcance real.

## Capítulo 140 — Process, hardware y diagnostics

- Getters de proceso/sistema, debugger y stack frame count.
- Entregable: frontera exacta entre introspección y process spawn.

---

# Volumen IX — Completar la semántica y cerrar correctness

## Capítulo 141 — Function pointers e interop genérico

- Imports dinámicos/static-resolved, callbacks y exception boundary.
- Entregable: llamada en ambos sentidos.

## Capítulo 142 — Aggregates C completos

- Fixed arrays, union, packing, aliases FFI y layout.
- Entregable: comparación independiente contra Clang/MSVC ABI.

## Capítulo 143 — `ref`, `out` e `in`

- Address passing, definite write, readonly reference y no-copy.
- Entregable: semántica y ABI.

## Capítulo 144 — Miembros completos de structs

- Constructors, properties, indexers, operators y receiver de value type.
- Entregable: evitar copiar el receiver accidentalmente.

## Capítulo 145 — Definite assignment y zero-init

- Scalars path-sensitive, aggregates deliberadamente zero-initialized y aggregate initializer.
- Entregable: leer basura deja de ser posible.

## Capítulo 146 — `Object` como raíz sin boxing

- Virtual ToString/Equals/GetHashCode para classes y synthesis estática para structs.
- Entregable: `object o = 5` sigue siendo error.

## Capítulo 147 — Ternario y `do...while`

- Typing, CFG y cleanup edges.
- Entregable: source→HIR/MIR y ejecución O0/O2.

## Capítulo 148 — `switch` correctamente tipado

- Case constants, enum underlying type, duplicate detection y control flow.
- Entregable: nested EH/loop fixtures.

## Capítulo 149 — `++/--`, compound assignment y `p->`

- Single evaluation, lvalue preservation y pointer member access.
- Entregable: side effects medidos.

## Capítulo 150 — Aritmética de constantes

- Literal width, signedness, promotions, `commonNumericType` y minimum signed.
- Entregable: mapa completo de casos y regresiones.

## Capítulo 151 — Shifts: una operación, dos roles

- Result type por operando izquierdo, normalization del count y backend.
- Entregable: resolver inconsistencias frontend/folder/lowering.

## Capítulo 152 — `stackalloc`, `sbyte` y punteros

- Type keyword ordering, evaluador, layout y LLVM.
- Entregable: evitar bugs por enums/rangos internos.

## Capítulo 153 — Parsing y formatting numérico

- Bases, signed/unsigned, float/double, hex, enums y overflow.
- Entregable: library algorithms con compiler-grade diagnostics.

## Capítulo 154 — Console por dos canales y formatting medido

- stdout/stderr, Write/WriteLine y tamaños exactos.
- Entregable: diagnostics que no dependen de C++ streams.

## Capítulo 155 — Views y escape checking

- StringView, Span, ReadOnlySpan, Optional y lifetimes.
- Entregable: lexer/buffers sin copias y sin dangling views.

## Capítulo 156 — I/O binaria, buffers y procesos hijos

- ByteBuffer, readers/writers, ReadAllBytes/WriteAllBytes y Process.Run.
- Entregable: compilar/linkear invocando una herramienta hija.

## Capítulo 157 — Arena allocator y algoritmos necesarios

- Medir usos reales en el compilador antes de diseñar toda STL.
- Entregable: lista mínima de algoritmos y arena por compilación.

## Capítulo 158 — Bugs de ownership, EH y valores extremos

- Double free, double destruction, catches, finally, minimum signed y owning slots.
- Entregable: una autopsia por bug con fixture previo y posterior.

## Capítulo 159 — Milestones CC1 y CC2

- Correctness semántico, memoria, type system y backend independence.
- Entregable: qué se cerró realmente y qué quedó abierto.

---

# Volumen X — Toolchain medible y versiones 0.7.x/0.8

## Capítulo 160 — Cuando “la suite está verde” no basta

- Binario equivocado, media matriz, logs temporales y tablas duplicadas.
- Entregable: reglas de evidencia dentro del repositorio.

## Capítulo 161 — `nscbuilder` y gate G00

- Etapas, casos, fingerprints, instalación y árbol inmóvil.
- Entregable: S0/S1 y built/installed identity.

## Capítulo 162 — 0.7.4: el gate descubre un orden inválido

- Por qué una herramienta de validación también puede mentir.
- Entregable: corrida fallida conservada y corrección metodológica.

## Capítulo 163 — 0.7.5: observar LLVM desde un supervisor

- stderr nativo, proceso padre/hijo y last observed pass.
- Entregable: NSC-LIMIT-005 reproducido.

## Capítulo 164 — Constraints cross-project

- Pérdida de identidad nominal, cache keys y módulo/namespace.
- Entregable: NSC-LIMIT-011.

## Capítulo 165 — 0.7.6: entrega grande de defectos de consumidores

- Enum bitwise, value/type collision, shift width, owned overwrite y constant folding.
- Entregable: un capítulo por familia si los deltas lo requieren.

## Capítulo 166 — Plan 0.8 frente a cierre 0.8

- Comparar gates diseñados con gates realmente aterrizados.
- Entregable: tabla `planeado / implementado / certificado / diferido`.

## Capítulo 167 — Arrays y temporales owned en 0.8

- Local owners, inferred lengths y borrowed temporary strings.
- Entregable: cierre de LIMIT-016/017/021.

## Capítulo 168 — Genéricos demand-driven y parámetros default

- Member reachability y call-site defaults.
- Entregable: LIMIT-018/020.

## Capítulo 169 — ABI, CodeView/PDB, COM y SIMD

- Narrar únicamente el subconjunto que los gates demuestren.
- Entregable: matrices productoras/consumidoras y artefactos debug.

## Capítulo 170 — NSLink como segunda autoridad

- Paridad con `lld-link`, mismos objetos y resultados comparables.
- Entregable: doble corrida sin confundir independencia del linker con independencia de LLVM.

## Capítulo 171 — Cierre 0.8.0-alpha

- Identidad, paquete, limitaciones deliberadas y siguiente frontera.
- Entregable: reconstrucción desde instalación limpia.

---

# Volumen XI — Bibliotecas precompiladas y NativeSharp 0.9

## Capítulo 172 — El problema de compilar por separado

- Lo que se pierde al no tener el source del productor.
- Entregable: lista mínima de contrato semántico/publicable.

## Capítulo 173 — NSMETA 1.3–1.5

- Evolución de records, type/member identity, layout, ownership y callbacks.
- Entregable: readers backward-compatible y rejection cases.

## Capítulo 174 — Consumo físico de `.lib` y DLL

- Archivo, import library, symbols y linker inputs.
- Entregable: productor/consumidor realmente separados.

## Capítulo 175 — Reconstrucción semántica desde metadata

- Imported types, members, overloads y diagnostics.
- Entregable: el consumidor compila sin source del productor.

## Capítulo 176 — Structs, enums y receivers precompilados

- Nominal composition transitiva, layouts y calls.
- Entregable: matriz cross-project.

## Capítulo 177 — Clases opacas y ownership entre módulos

- Handles/references, construction/destruction y transfer.
- Entregable: lifetime observable sin conocer layout privado.

## Capítulo 178 — Callbacks `extern C delegate`

- Signature transport, thunk y host callback.
- Entregable: callback desde producto precompilado.

## Capítulo 179 — Regla de generación por pasada

- Una sola fuente de verdad, deduplicación y orden determinista.
- Entregable: eliminar tablas copiadas y medir imports/operations.

## Capítulo 180 — Cierre de `nsc 0.9.0`

- Productos, NSMETA 1.5, evidencia y promesas explícitamente ausentes.
- Entregable: baseline local 0.9.

---

# Volumen XII — Congelar el contrato de 1.0

## Capítulo 181 — 1.0-E1: objetos precompilados completos

- Public object descriptors, constructors/destructors importados y problema PE inesperado.
- Entregable: consumer de objetos completo.

## Capítulo 182 — 1.0-E2: genéricos entre bibliotecas

- Public templates, materialization/reuse, constraints y nominal identity.
- Entregable: monomorfización cross-library.

## Capítulo 183 — 1.0-E3: delegate cell y closure lifetime ABI

- Context ownership, destroy callback y normal/exception cleanup.
- Entregable: closures cruzando DLL sin fuga/double free.

## Capítulo 184 — 1.0-E3: excepciones entre módulos

- RTTI, current exception, runtime boundary y typed catch cross-DLL.
- Entregable: throw en productor, catch en consumidor.

## Capítulo 185 — 1.0-E4: inventariar lo que debe congelarse

- Layouts, symbols, call contracts, ownership, exceptions, delegates y metadata.
- Entregable: ABI surface manifest.

## Capítulo 186 — Native ABI 1.0 y Runtime ABI 1.0

- Qué cambia major/minor, qué jamás cambia silenciosamente.
- Entregable: política formal de compatibilidad.

## Capítulo 187 — NSMETA 1.8

- Campos, fingerprints, checksums, public/internal symbols y backward compatibility.
- Entregable: golden files y corruptions.

## Capítulo 188 — Golden ABI y diagnósticos de rechazo

- Comparar contratos antes de link; mensajes accionables.
- Entregable: incompatibilidades intencionales.

## Capítulo 189 — Reparaciones descubiertas por la matriz E4

- Expectations obsoletas de Stages 42/47/48/49 y por qué no se relajaron a “cualquier cantidad”.
- Entregable: tests precisos actualizados.

## Capítulo 190 — Baseline G00 de E1–E4

- 11/11 etapas, 324 casos, 1013 archivos, hashes S0/S1 y built/installed.
- Entregable: reproducir el gate y explicar exactamente qué prueba.

---

# Volumen XIII — De 0.9 certificado al producto 1.0.0

Los capítulos 191–199 están `PLANEADOS` en el estado observado. Se completarán con hechos reales y
podrán cambiar de número o dividirse conforme aterrice el release.

## Capítulo 191 — Ampliar el corpus consumidor

- `.lib`, DLL, objects, generic libraries, closures, exceptions y ownership combinados.
- Entregable: proyectos externos que no dependan del checkout del compilador.

## Capítulo 192 — Diagnósticos ABI de calidad de producto

- Version mismatch, missing field, symbol mismatch, layout/fingerprint y remediation.
- Entregable: golden diagnostics sin crashes tardíos del linker.

## Capítulo 193 — Instalación limpia y descubrimiento del SDK

- Paths, runtime/Core/metadata, linker selection y ausencia de dependencias accidentales.
- Entregable: máquina/directorio limpio.

## Capítulo 194 — Compatibilidad Debug/Release y O0/O2

- Mismo significado, diferentes optimizaciones, EH/ownership/layout invariants.
- Entregable: matriz cruzada productor/consumidor.

## Capítulo 195 — Compatibilidad con `lld-link` y NSLink

- Mismos inputs, productos válidos y diferencias permitidas.
- Entregable: doble linker dentro del gate final.

## Capítulo 196 — Release candidate 1

- Freeze de features, triage, package identity, changelog y known limitations.
- Entregable: `nsc 1.0.0-rc.1` instalado y certificado.

## Capítulo 197 — Periodo de estabilización

- Clasificación de bugs, backport policy, ABI freeze y actualización de corpus.
- Entregable: diario de cada corrección independiente de Git.

## Capítulo 198 — Gate final 1.0

- Source immutability, clean build, toolchains, sanitizers, two linkers, package install,
  producer/consumer matrix, hashes y archive.
- Entregable: acta verificable de publicación.

## Capítulo 199 — NativeSharp 1.0.0

- Qué promete y qué no promete el producto.
- Cómo crear, compilar, depurar y distribuir un proyecto real.
- Entregable: identidad exacta, artifacts, manifests y tutorial de instalación limpia.

---

# Volumen XIV — Atlas de reconstrucción y referencia

## Capítulo 200 — Atlas del árbol final

- Responsabilidad de cada directorio, library y executable.
- Enlaces hacia el capítulo donde nació cada archivo importante.

## Capítulo 201 — Atlas de tipos y conversiones

- Tabla consolidada, con referencia a la etapa que introdujo o corrigió cada regla.

## Capítulo 202 — Atlas de AST, HIR y MIR

- Cada node/instruction kind, operands, invariants y lowering.

## Capítulo 203 — Atlas de diagnostics

- IDs estables, condición, ejemplo mínimo y primer capítulo donde aparece.

## Capítulo 204 — Atlas del ABI y layouts

- Target primitives, objects, structs, arrays, delegates, closures, exceptions y metadata.

## Capítulo 205 — Atlas del runtime e imports Win32

- Cada hook, firma, caller, import requerido y capítulo de origen.

## Capítulo 206 — Atlas de pruebas y gates

- Qué suite protege qué contrato; positivos, negativos, structural y execution.

## Capítulo 207 — Atlas de bugs y recuperaciones

- Síntoma, root cause, fix, regression y versiones afectadas.

## Capítulo 208 — Recetas de reconstrucción por checkpoint

- Desde `C000` hasta `C199`, comandos y resultados esperados.

## Capítulo 209 — Glosario y concordancia histórica

- Nombres antiguos/nuevos, stage/product/ABI, términos españoles/ingleses y símbolos.

## Capítulo 210 — Lo que queda después de 1.0

- Sólo planes explícitamente documentados: self-hosting, mayor independencia del toolchain,
  nuevas plataformas o features, sin presentarlos como parte de 1.0.

---

# Apéndices obligatorios

## A — Catálogo de conversaciones

Turnos, adjuntos, hashes, claims, decisiones y relaciones con capítulos.

## B — Catálogo de Markdown

Los 170 documentos editoriales iniciales y los reportes relevantes, deduplicados pero nunca
descartados sin registro.

## C — Catálogo de snapshots y ZIP

Nombre, SHA-256 citado/calculado, manifest, identidad, build y tests.

## D — Deltas reconstruibles

Patch por checkpoint; para etapas recuperadas por inferencia, receta manual y nivel de confianza.

## E — Outputs dorados

AST/HIR/MIR/LLVM/COFF/PE, diagnostics y CLI.

## F — Matrices de conformidad

Lenguaje, ABI, ownership, EH, producer/consumer, toolchains y linkers.

## G — Bibliografía técnica

LLVM, Microsoft x64 ABI, COFF/PE, SEH y algoritmos de compiladores. La bibliografía explicará el
contexto; no sustituirá la evidencia del proyecto.

## H — Índice de símbolos del código

Clase, método, archivo actual y capítulo de nacimiento.

## I — Índice de decisiones

Decision records con alternativas, consecuencias y revisiones posteriores.

## J — Índice de deuda y límites deliberados

Qué quedó fuera en cada checkpoint y cuándo se resolvió o se mantuvo fuera de 1.0.

## K — Catálogo de paquetes y duplicados

Hash crudo, hash de árbol, equivalencias normalizadas, variantes FULL/SOURCE_ONLY e identidad de
copias con sufijos.

## L — Genealogía física

Padres materiales, consolidaciones, recoveries, candidatos y nivel de confianza de cada vínculo.

## M — Registro de certificaciones

Resultados históricos y reproducidos separados por paquete, plataforma, toolchain y universo de
pruebas.

## N — Anomalías del archivo

Huecos nominales, versiones regresivas, paquetes vacíos, builds incluidos y descripciones
obsoletas.

## O — Concordancia de versiones

Versión de producto, lenguaje, stage, Native ABI, Runtime ABI, NSMETA, gate y build identity.

## P — Reconstrucciones pedagógicas

Estados necesarios para enseñar paso a paso que no sobrevivieron como paquete material, con su
prueba de convergencia.

## Q — Proveniencia de conversaciones

Decisiones extraídas de chats, fecha, contexto, archivos afectados y corroboración en código o
tests.
